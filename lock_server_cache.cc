// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"
#include <algorithm>
using namespace std;

lock_server_cache::lock_server_cache()
{
  pthread_mutex_init(&ls_mutex, 0);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, 
                               std::string id, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  string send_cid = "";

  pthread_mutex_lock(&ls_mutex);
  lock_info info = ls_map[lid];
  if(info.cid == "" || info.cid == id)
  {
    info.cid = id;
    if(info.q.size() != 0) r = 1;
    else r = 0;
    ls_map[lid] = info;
    printf("[%s] request lock:%u nd_return:%u\n", id.c_str(), int(lid), r);
  }
  else
  {
    if(info.q.size() == 0)
     send_cid = info.cid;
    if(find(info.q.begin(), info.q.end(), id) == info.q.end())
      info.q.push_back(id);
    ls_map[lid] = info;
    ret = lock_protocol::RETRY;
    printf("[%s] lock:%u holdby:%s\n", id.c_str(), int(lid), info.cid.c_str());
  }
  pthread_mutex_unlock(&ls_mutex);

  if(send_cid != "")
     send_retry(send_cid, lid, 1);

  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  string send_cid = "";
  int send_nd_return = 0;

  pthread_mutex_lock(&ls_mutex);
  lock_info info = ls_map[lid];
  if(info.cid == id)
  {
    printf("[%s] release lock:%u\n", id.c_str(), int(lid));
    if(info.q.size() == 0)
      info.cid = "";
    else
    {
      info.cid = info.q[0];
      info.q.erase(info.q.begin());
      send_cid = info.cid;
      send_nd_return = (info.q.size() != 0)?1:0;
    }
    ls_map[lid] = info;
  }
  pthread_mutex_unlock(&ls_mutex);

  if(send_cid != "")
    send_retry(send_cid, lid, send_nd_return);

  return lock_protocol::OK;
}

void lock_server_cache::send_retry(const string &id, 
                                   lock_protocol::lockid_t lid, int nd_return)
{
  int r;
  printf("[%s] retry lock:%u nd_return:%u\n", id.c_str(), int(lid), nd_return);
  handle h(id);
  rpcc *cl = h.safebind();
  cl->call(rlock_protocol::retry, lid, nd_return, r);
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

