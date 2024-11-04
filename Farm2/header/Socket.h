#ifndef Socket_H
#define Socket_H

//-----------------------------DICHIARAZIONE FUNZIONI-------------------------------------
void connectToServer();
void sendToServer(long sum, char *filename);
void writeEndSignal();
void openServer(int *fd_skt);
int receiveFromClient(long *num, char **filename);
#endif 