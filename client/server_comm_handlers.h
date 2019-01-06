#ifndef SERVER_COMM_HANDLERS_H
#define SERVER_COMM_HANDLERS_H

#include <string>

void LoginApproved(const std::string& str);
void LoginRejected(const std::string& str);
void LoginInvHWID(const std::string& str);
void LoginInvIP(const std::string& str);
void CheatLibraryReceive(const std::string& str);
void SettingsLibraryReceive(const std::string& str);
void LibraryAllocate(const std::string& str);
void SubscriptionList(const std::string& str);

#endif
