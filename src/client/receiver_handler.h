#pragma once
void *receiver_thread(void *arg);
void  dispatch_server_message(int type, const char *name, const char *text);
