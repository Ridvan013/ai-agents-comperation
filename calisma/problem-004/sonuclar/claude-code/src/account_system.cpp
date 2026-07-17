#include "account_system.hpp"

namespace bookstore {

namespace {
/// Account privileges named by the spec. {0} denotes "nobody logged in" and is
/// therefore not a level an account can be created with.
bool isValidPrivilege(int privilege) {
  return privilege == 1 || privilege == 3 || privilege == 7;
}
}  // namespace

void AccountSystem::open() {
  const bool firstRun = users_.open("users.dat", EmptyMeta{0});
  index_.open("users.idx");
  if (firstRun) create("root", "sjtu", "root", 7);
}

void AccountSystem::close() {
  users_.close();
  index_.close();
}

int AccountSystem::locate(const std::string &userid, UserRecord &out) {
  int id;
  if (!index_.find(AccountStr(userid), id)) return -1;
  users_.read(id, out);
  return id;
}

void AccountSystem::create(const std::string &userid, const std::string &password,
                           const std::string &username, int privilege) {
  UserRecord record;
  record.userid = AccountStr(userid);
  record.password = AccountStr(password);
  record.username = AccountStr(username);
  record.privilege = privilege;

  int id = users_.allocate();
  users_.write(id, record);
  index_.insert(record.userid, id);
}

bool AccountSystem::isLoggedIn(const std::string &userid) const {
  for (const LoginFrame &frame : stack_)
    if (frame.userid == userid) return true;
  return false;
}

void AccountSystem::su(const std::vector<std::string> &tokens) {
  failIf(tokens.size() != 2 && tokens.size() != 3);
  failIf(!isValidIdentifier(tokens[1]));

  UserRecord user;
  failIf(locate(tokens[1], user) == -1);

  if (tokens.size() == 3) {
    // An explicitly supplied password must match even for a privileged caller.
    failIf(!isValidIdentifier(tokens[2]));
    failIf(!(user.password == AccountStr(tokens[2])));
  } else {
    // Omitting it is only allowed when strictly outranking the target.
    failIf(currentPrivilege() <= user.privilege);
  }

  stack_.push_back(LoginFrame{tokens[1], user.privilege, -1});
}

void AccountSystem::logout(const std::vector<std::string> &tokens) {
  failIf(tokens.size() != 1);
  failIf(stack_.empty());
  stack_.pop_back();
}

void AccountSystem::registerAccount(const std::vector<std::string> &tokens) {
  failIf(tokens.size() != 4);
  failIf(!isValidIdentifier(tokens[1]));
  failIf(!isValidIdentifier(tokens[2]));
  failIf(!isValidUsername(tokens[3]));

  UserRecord existing;
  failIf(locate(tokens[1], existing) != -1);
  create(tokens[1], tokens[2], tokens[3], 1);
}

void AccountSystem::changePassword(const std::vector<std::string> &tokens) {
  failIf(tokens.size() != 3 && tokens.size() != 4);
  failIf(!isValidIdentifier(tokens[1]));

  UserRecord user;
  int id = locate(tokens[1], user);
  failIf(id == -1);

  const std::string &newPassword = tokens.back();
  failIf(!isValidIdentifier(newPassword));

  if (tokens.size() == 4) {
    failIf(!isValidIdentifier(tokens[2]));
    failIf(!(user.password == AccountStr(tokens[2])));
  } else {
    failIf(currentPrivilege() != 7);  // Only the owner may skip the old password.
  }

  user.password = AccountStr(newPassword);
  users_.write(id, user);
}

void AccountSystem::addAccount(const std::vector<std::string> &tokens) {
  failIf(tokens.size() != 5);
  failIf(!isValidIdentifier(tokens[1]));
  failIf(!isValidIdentifier(tokens[2]));
  failIf(tokens[3].size() != 1 || !isDigit(tokens[3][0]));
  failIf(!isValidUsername(tokens[4]));

  const int privilege = tokens[3][0] - '0';
  failIf(!isValidPrivilege(privilege));
  failIf(privilege >= currentPrivilege());

  UserRecord existing;
  failIf(locate(tokens[1], existing) != -1);
  create(tokens[1], tokens[2], tokens[4], privilege);
}

void AccountSystem::deleteAccount(const std::vector<std::string> &tokens) {
  failIf(tokens.size() != 2);
  failIf(!isValidIdentifier(tokens[1]));

  UserRecord user;
  int id = locate(tokens[1], user);
  failIf(id == -1);
  failIf(isLoggedIn(tokens[1]));

  index_.erase(user.userid);
  users_.deallocate(id);
}

}  // namespace bookstore
