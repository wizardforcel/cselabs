// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include <time.h>
#include <vector>
#include <unistd.h>
using namespace std;


int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     i_lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  pthread_mutex_init(&lc_mutex, 0);
  pthread_create(&tid, 0, &lock_client_cache::thread_func, this);
}


lock_client_cache::~lock_client_cache()
{
  pthread_cancel(tid);
  pthread_join(tid, 0);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{

  /*
   *0. accuqire global lock
   *1. if not in map, create and mark UNALLOC
   *2. get and judge status
   *3. if UNALLOC, mark WAIT and ask server for it
   *  3.1 if server return OK, mark it OCCUPY and mark nd_return,
   *      relese global lock and exit function
   *  3.2 if not, release global lock and return step 0
   *4. if AVAIL, mark OCCUPY, release global lock and exit function
   *5. else release global lock and return step 0
   */

  int ret = lock_protocol::RETRY;

  do {
    pthread_mutex_lock(&lc_mutex);
    
    lock_info info = lc_map[lid];
    if(info.status == UNALLOC)
    {
      info.status = WAIT;
      int r;
      ret = cl->call(lock_protocol::acquire, lid, id, r);
      if(ret == lock_protocol::OK)
      {
        //printf("[%s] request lock:%u res:%u nd_return:%u\n",
        //       id.c_str(), int(lid), int(ret), r);
        info.status = OCCUPY;
        info.nd_return = (r == 1);
      }
      lc_map[lid] = info;
    }
    else if(info.status == AVAIL)
    {
      //printf("[%s] acquire lock:%u\n", id.c_str(), int(lid));
      info.status = OCCUPY;
      lc_map[lid] = info;
      ret = lock_protocol::OK;
    }
    //WAIT & OCCUPY do nothing

    pthread_mutex_unlock(&lc_mutex);
  } while(ret == lock_protocol::RETRY);

  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  pthread_mutex_lock(&lc_mutex);
  lock_info info = lc_map[lid];
  info.status = AVAIL;
  info.last_release = time(0);
  lc_map[lid] = info;
  pthread_mutex_unlock(&lc_mutex);
  //printf("[%s] release lock:%u\n", id.c_str(), int(lid));
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &r)
{
  //no use 
  int ret = rlock_protocol::OK;
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int nd_return, int &r)
{
  //svr inform client when 1.someone need the lock 2.when lock is OK
  pthread_mutex_lock(&lc_mutex);
  lock_info info = lc_map[lid];
  if(info.status == UNALLOC ||
     info.status == WAIT)
    info.status = AVAIL; 
  info.nd_return = (nd_return == 1);
  lc_map[lid] = info;
  pthread_mutex_unlock(&lc_mutex);
  //printf("[%s] retry lock:%u nd_return:%u\n", id.c_str(), int(lid), nd_return);
  return rlock_protocol::OK;
}

void *lock_client_cache::thread_func(void *data)
{
  lock_client_cache *p = (lock_client_cache *)data;

  while(true)
  {
    sleep(1);
    pthread_mutex_lock(&(p->lc_mutex));
    vector<lock_protocol::lockid_t> nd_release;
    for(map<lock_protocol::lockid_t, lock_info>::iterator it
        = p->lc_map.begin(); it != p->lc_map.end(); ++it)
    {
      if(it->second.nd_return && 
         it->second.status == AVAIL /*&& 
         it->second.last_release < time(0)*/)
      {
        it->second.status = OCCUPY;
        nd_release.push_back(it->first);
        //printf("[%s] recycle lock:%u\n",
        //       p->id.c_str(), int(it->first));
      }
    }
    pthread_mutex_unlock(&(p->lc_mutex));

    for(vector<lock_protocol::lockid_t>::iterator it
        = nd_release.begin(); it != nd_release.end(); ++it)
    {
      int r;
      p->call_do_release(*it);
      p->cl->call(lock_protocol::release, *it, p->id, r);
      //printf("[%s] do release:%u\n", p->id.c_str(), int(*it));
      pthread_mutex_lock(&(p->lc_mutex));
      p->lc_map[*it].status = UNALLOC;
      p->lc_map[*it].nd_return = 0;
      pthread_mutex_unlock(&(p->lc_mutex));
    }
  }
  
  return 0;
}


