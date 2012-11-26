#include "messages.h" 
#include "network.h" 

int main(void) {
  struct RequestPageMessage msg;
  msg.type = READ;
  msg.pg_address = (void *) 0xcafebebe;
  msg.from = 1;
  
  sendReqPgMsg(&msg, 0); 
}
