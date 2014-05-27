// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
using namespace std;

extent_client::extent_client(string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

// a demo to show how to use RPC
extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::getattr, eid, attr);
  return ret;
}

extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  ret = cl->call(extent_protocol::create, type, id);
  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  ret = cl->call(extent_protocol::get, eid, buf);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  int res;
  ret = cl->call(extent_protocol::put, eid, buf, res);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  int res;
  ret = cl->call(extent_protocol::remove, eid, res);
  return ret;
}

///////////////////////////////////////////////////////////////////////

extent_client_cache::extent_client_cache(string dst)
  : extent_client(dst)
{
  pthread_mutex_init(&ec_mutex, 0);
}

extent_protocol::status 
extent_client_cache::modify_attr_get(extent_protocol::extentid_t eid, time_t tm)
{
  extent_protocol::status ret = extent_protocol::OK;
  extent_protocol::attr a;
  if(cache_attr.find(eid) == cache_attr.end())
  {
    ret = cl->call(extent_protocol::getattr, eid, a);
    if(ret != extent_protocol::OK) return ret;
  }
  else a = cache_attr[eid];
  a.atime = tm;
  cache_attr[eid] = a;
  return ret;
}

extent_protocol::status
extent_client_cache::modify_attr_put(extent_protocol::extentid_t eid, size_t sz, time_t tm)
{
  extent_protocol::status ret = extent_protocol::OK;
  extent_protocol::attr a;
  if(cache_attr.find(eid) == cache_attr.end())
  {
    ret = cl->call(extent_protocol::getattr, eid, a);
    if(ret != extent_protocol::OK) return ret;
  }
  else a = cache_attr[eid];
  a.atime = a.ctime = a.mtime = tm;
  a.size = sz;
  cache_attr[eid] = a;
  return ret;
}

extent_protocol::status
extent_client_cache::get(extent_protocol::extentid_t eid, string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  pthread_mutex_lock(&ec_mutex);
  if(cache_content.find(eid) == cache_content.end())
  {
    ret = cl->call(extent_protocol::get, eid, buf);
    printf("Get Request: %llu res: %d\n", eid, ret);
    if(ret == extent_protocol::OK)
    {
      cache_content[eid] = buf;
    }
  }
  else
  {
    printf("Get: %llu\n", eid);
    buf = cache_content[eid];
  }
  ret = modify_attr_get(eid, time(0));
  pthread_mutex_unlock(&ec_mutex);
  return ret;
}

extent_protocol::status
extent_client_cache::create(uint32_t type,
                            extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::create, type, id);
  if(ret == extent_protocol::OK)
  {
    pthread_mutex_lock(&ec_mutex);
    cache_content[id] = "";
    extent_protocol::attr a;
    time_t tm = time(0);
    a.atime = a.ctime = a.mtime = tm;
    a.type = type;
    a.size = 0;
    cache_attr[id] = a;
    pthread_mutex_unlock(&ec_mutex);
  }
  printf("Create: %llu res: %d\n", id, ret);
  return ret;
}

extent_protocol::status
extent_client_cache::getattr(extent_protocol::extentid_t eid, 
		             extent_protocol::attr &a)
{
  extent_protocol::status ret = extent_protocol::OK;
  pthread_mutex_lock(&ec_mutex);
  if(cache_attr.find(eid) == cache_attr.end())
  {
    ret = cl->call(extent_protocol::getattr, eid, a);
    printf("Getattr Request: %llu\n", eid);
    if(ret == extent_protocol::OK)
      cache_attr[eid] = a;
  }
  else
  {
    printf("Getattr: %llu\n", eid);
    a = cache_attr[eid];
  }
  pthread_mutex_unlock(&ec_mutex);
  return ret;
}

extent_protocol::status
extent_client_cache::put(extent_protocol::extentid_t eid, string buf)
{
  printf("Put %llu\n", eid);
  extent_protocol::status ret = extent_protocol::OK;
  pthread_mutex_lock(&ec_mutex);
  cache_content[eid] = buf;
  ret = modify_attr_put(eid, buf.size(), time(0));
  pthread_mutex_unlock(&ec_mutex);
  return ret;
}

extent_protocol::status
extent_client_cache::remove(extent_protocol::extentid_t eid)
{
  int res;
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::remove, eid, res);
  if(ret == extent_protocol::OK)
  {
    pthread_mutex_lock(&ec_mutex);
    map<extent_protocol::extentid_t, string>::iterator it1
      = cache_content.find(eid);
    if(it1 != cache_content.end())
      cache_content.erase(it1);
    map<extent_protocol::extentid_t,
        extent_protocol::attr>::iterator it2
      = cache_attr.find(eid);
    if(it2 != cache_attr.end())
      cache_attr.erase(it2);
    pthread_mutex_unlock(&ec_mutex);
  }
  printf("Remove: %llu\n", eid);
  return ret;
}

void
extent_client_cache::do_release(extent_protocol::extentid_t eid)
{
  printf("Release: %llu\n", eid);
  int res;
  pthread_mutex_lock(&ec_mutex);
  map<extent_protocol::extentid_t, string>::iterator it1
      = cache_content.find(eid);
  if(it1 != cache_content.end())
  {
    cl->call(extent_protocol::put, eid, it1->second, res);
    cache_content.erase(it1);
  }
  map<extent_protocol::extentid_t,
      extent_protocol::attr>::iterator it2
    = cache_attr.find(eid);
  if(it2 != cache_attr.end())
    cache_attr.erase(it2);
  pthread_mutex_unlock(&ec_mutex);
}

