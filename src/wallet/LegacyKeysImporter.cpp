#include "LegacyKeysImporter.h"

#include <vector>
#include <system_error>

#include "common/StringTools.h"

#include "core/Currency.h"
#include "core/Account.h"
#include "base/CryptoNoteTools.h"

#include "Serialization/SerializationTools.h"

#include "wallet_legacy/WalletLegacySerializer.h"
#include "wallet_legacy/WalletUserTransactionsCache.h"
#include "WalletUtils.h"
#include "WalletErrors.h"

using namespace Crypto;

namespace {

struct keys_file_data {
  chacha8_iv iv;
  std::string account_data;

  void serialize(CryptoNote::ISerializer& s) {
    s(iv, "iv");
    s(account_data, "account_data");
  }
};

void loadKeysFromFile(const std::string& filename, const std::string& password, CryptoNote::AccountBase& account) {
  keys_file_data keys_file_data;
  std::string buf;

  if (!Common::loadFileToString(filename, buf)) {
    throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR), "failed to load \"" + filename + '\"');
  }

  if (!CryptoNote::fromBinaryArray(keys_file_data, Common::asBinaryArray(buf))) {
    throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR), "failed to deserialize \"" + filename + '\"');
  }

  chacha8_key key;
  cn_context cn_context;
  generate_chacha8_key(cn_context, password, key);
  std::string account_data;
  account_data.resize(keys_file_data.account_data.size());
  chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);

  if (!CryptoNote::loadFromBinaryKeyValue(account, account_data)) {
    throw std::system_error(make_error_code(CryptoNote::error::WRONG_PASSWORD));
  }

  const CryptoNote::AccountKeys& keys = account.getAccountKeys();
  CryptoNote::throwIfKeysMissmatch(keys.viewSecretKey, keys.address.viewPublicKey);
  CryptoNote::throwIfKeysMissmatch(keys.spendSecretKey, keys.address.spendPublicKey);
}

}

namespace CryptoNote {

void importLegacyKeys(const std::string& legacyKeysFilename, const std::string& password, std::ostream& destination) {
  CryptoNote::AccountBase account;

  loadKeysFromFile(legacyKeysFilename, password, account);

  CryptoNote::WalletUserTransactionsCache transactionsCache;
  std::string cache;
  CryptoNote::WalletLegacySerializer importer(account, transactionsCache);
  importer.serialize(destination, password, false, cache);
}

} //namespace CryptoNote
