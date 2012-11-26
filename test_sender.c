#include "messages.h" 
#include "sender.h" 

int main(void) {
  struct RequestPageMessage msg;
  msg.type = READ;
  msg.pg_address = (void *) 0xcafebebe;
  msg.from = 2;
  
  send_to_client(0, &msg, sizeof(msg));
}
