// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
using namespace std;

lock_server::lock_server()
  : nacquire (0)
{
  pthread_mutex_init(&ls_mutex, NULL);
}

lock_server::~lock_server()
{
  pthread_mutex_destroy(&ls_mutex);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;

  do {
  pthread_mutex_lock(&ls_mutex);
  
  /*map<lock_protocol::lockid_t, int>::iterator it
    = ls_map.find(lid);
  if(it == ls_map.end())*/
  if(ls_map[lid] == 0)
  {
    ls_map[lid] = clt;
    ret = lock_protocol::OK;
  }
  else
  {
    ret = lock_protocol::RETRY;
  }

  pthread_mutex_unlock(&ls_mutex);
  } while(ret == lock_protocol::RETRY);

  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;

  pthread_mutex_lock(&ls_mutex);

  /*map<lock_protocol::lockid_t, int>::iterator it
    = ls_map.find(lid);
  if(it == ls_map.end())
  {
    ret = lock_protocol::IOERR;
  }
  else
  {
    ls_map.erase(it);
    ret = lock_protocol::OK;
  }*/
  ls_map[lid] = 0;

  pthread_mutex_unlock(&ls_mutex);
  return ret;
}
