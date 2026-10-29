#include "aegis/ota/ab_slot_manager.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <vector>

#include "aegis/crypto/sha256.h"

namespace aegis::ota {

namespace {

Result<std::vector<uint8_t>> read_image_bytes(hal::IStorageHal& storage,
                                              std::string_view image_path) {
  if (storage.exists(image_path)) {
    return storage.read_file(image_path);
  }
  std::ifstream ifs{std::string(image_path), std::ios::binary};
  if (!ifs) {
    return Result<std::vector<uint8_t>>::Error(
        ErrorCode::kIoError,
        "OTA image file not found: " + std::string(image_path));
  }
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
  if (ifs.bad()) {
    return Result<std::vector<uint8_t>>::Error(ErrorCode::kIoError,
                                               "Failed reading OTA image");
  }
  return Result<std::vector<uint8_t>>::Ok(std::move(bytes));
}

std::string to_lower_hex(std::string_view hex) {
  std::string out;
  out.reserve(hex.size());
  for (char c : hex) {
    out.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

}  // namespace

AbSlotManager::AbSlotManager(hal::IBootControlHal& boot_control,
                             hal::IStorageHal& storage)
    : boot_control_(boot_control), storage_(storage) {}

std::string AbSlotManager::other_slot(std::string_view slot) {
  return (slot == "A") ? "B" : "A";
}

Status AbSlotManager::init() {
  auto loaded = boot_control_.load_state();
  if (loaded.ok()) {
    state_ = loaded.value();
    return Status::Ok();
  }
  state_ = hal::BootSlotState{};
  state_.active_slot = "A";
  state_.marked_good = true;
  return boot_control_.save_state(state_);
}

Status AbSlotManager::stage(std::string_view image_path,
                            std::string_view expected_sha256_hex) {
  auto digest_check = crypto::Sha256::from_hex(expected_sha256_hex);
  if (!digest_check.ok()) {
    return Status::Error(ErrorCode::kInvalidArgument,
                         "Invalid expected SHA-256 hex format");
  }

  auto img_res = read_image_bytes(storage_, image_path);
  if (!img_res.ok()) {
    return img_res.status();
  }

  const std::string actual_hex = crypto::Sha256::hash_hex(img_res.value());
  if (actual_hex != to_lower_hex(expected_sha256_hex)) {
    return Status::Error(ErrorCode::kIntegrityError,
                         "OTA image SHA-256 mismatch");
  }

  const std::string target_slot = other_slot(state_.active_slot);
  state_.pending_slot = target_slot;
  state_.boot_attempts_left = kDefaultBootAttempts;
  state_.marked_good = false;
  state_.staged_sha256 = actual_hex;
  state_.staged_image = std::string(image_path);

  return boot_control_.save_state(state_);
}

Status AbSlotManager::on_boot() {
  if (state_.pending_slot.empty()) {
    return Status::Ok();
  }

  // First boot into newly staged pending slot: switch active_slot to pending
  if (state_.active_slot != state_.pending_slot) {
    state_.active_slot = state_.pending_slot;
  }

  if (state_.marked_good) {
    state_.pending_slot.clear();
    state_.boot_attempts_left = 0;
    return boot_control_.save_state(state_);
  }

  if (state_.boot_attempts_left > 0) {
    --state_.boot_attempts_left;
    return boot_control_.save_state(state_);
  }

  // Boot attempts exhausted without mark_good() -> automatic rollback!
  state_.active_slot = other_slot(state_.pending_slot);
  state_.pending_slot.clear();
  state_.boot_attempts_left = 0;
  state_.marked_good = true;
  return boot_control_.save_state(state_);
}

Status AbSlotManager::mark_good() {
  state_.marked_good = true;
  state_.pending_slot.clear();
  state_.boot_attempts_left = 0;
  return boot_control_.save_state(state_);
}

}  // namespace aegis::ota
