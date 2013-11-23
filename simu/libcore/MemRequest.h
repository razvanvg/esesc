// Contributed by Jose Renau
//
// The ESESC/BSD License
//
// Copyright (c) 2005-2013, Regents of the University of California and 
// the ESESC Project.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   - Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
//   - Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
//   - Neither the name of the University of California, Santa Cruz nor the
//   names of its contributors may be used to endorse or promote products
//   derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef MEMREQUEST_H
#define MEMREQUEST_H

#include "MemObj.h"
#include "nanassert.h"

/* }}} */

#ifdef DEBUG
#define DEBUG_CALLPATH 1
#endif

class DInst;

class MemRequest {
 public:
#ifdef DEBUG
  uint64_t     id;
#endif
 private:
  static pool<MemRequest> actPool;
  friend class pool<MemRequest>;
 protected:
  /* msgType declarations {{{1 */
  enum msgType {
    msg_read,
    msg_write,
    msg_writeAddress,
    msg_writeback,
    msg_invalidate,
    msg_vpcwriteupdate
  };
#ifdef DEBUG_CALLPATH
  class CallEdge {
  public:
    const MemObj *s;     // start
    const MemObj *e;     // end
    TimeDelta_t   tismo; // Time In Start Memory Object
    msgType       type;  // msgType
    bool          child; // msg generated by a child
  };
#endif
  /* }}} */

  /* Local variables {{{1 */
  AddrType     addr;
  AddrType     pc; //for VPC updates
  DataType     data;  
  enum msgType type;

  MemObj       *homeMemObj; // Starting home node
  MemObj       *currMemObj;
 protected:
#ifdef DEBUG_CALLPATH
  MemObj       *prevMemObj;
  std::vector<CallEdge> calledge;
  Time_t        lastCallTime;
#endif

  MemRequest   *parent_req; // Only used for invalidate to track pending acks
  int16_t       pending;  // pending 
  uint16_t      lineSize; // Request size, originally same as MemObj unless overruled

  CallbackBase *cb;

  Time_t        startClock;

  bool          prefetch;
  bool          mmu;

  bool          capInvalidate;
  bool          vpc_update;

  // Being use specific payload (Try to remove whenever you finish your paper)
  DInst        *dinst;

  /* }}} */

  MemRequest();
  virtual ~MemRequest();

  // processor direct requests
  void read();
  void write();
  void writeAddress();

  // DOWN
  void busRead();
  void pushDown();

  // UP
  void pushUp();
  void invalidate();
 
  StaticCallbackMember0<MemRequest, &MemRequest::read>          readCB;
  StaticCallbackMember0<MemRequest, &MemRequest::write>         writeCB;
  StaticCallbackMember0<MemRequest, &MemRequest::writeAddress>  writeAddressCB;

  StaticCallbackMember0<MemRequest, &MemRequest::busRead>    busReadCB;
  StaticCallbackMember0<MemRequest, &MemRequest::pushDown>   pushDownCB;

  StaticCallbackMember0<MemRequest, &MemRequest::pushUp>     pushUpCB;
  StaticCallbackMember0<MemRequest, &MemRequest::invalidate> invalidateCB;

#ifdef DEBUG_CALLPATH
  void dump_calledge(TimeDelta_t lat);
public:
  void rawdump_calledge(TimeDelta_t lat=0, Time_t total=0);
protected:
  void upce(TimeDelta_t lat);
  void upceAbs(Time_t when) {
    upce(when-globalClock);
  };
#else
  void dump_calledge(TimeDelta_t lat) {
  }
  void upce(TimeDelta_t lat) {
  };
  void upceAbs(Time_t when) {
  };
#endif
  static MemRequest *create(MemObj *m, AddrType addr, CallbackBase *cb);
 public:
  static MemRequest *createFillRead(MemObj *m, DInst *dinst, AddrType addr, CallbackBase *cb=0) { 
    MemRequest *mreq = create(m,addr,cb);
    mreq->type       = msg_read;
    mreq->parent_req = 0;
    mreq->dinst      = dinst;
    return mreq;
  }
  static MemRequest *createRead(MemObj *m, DInst *dinst, AddrType addr, CallbackBase *cb=0) { 
    //I(addr == dinst->getPC() || addr == dinst->getAddr());
    MemRequest *mreq = create(m,addr,cb);
    mreq->type       = msg_read;
    mreq->parent_req = 0;
    mreq->dinst = dinst; // FERMI
    return mreq;
  }
  static MemRequest *createWrite(MemObj *m, AddrType addr, CallbackBase *cb=0) {
    MemRequest *mreq = create(m,addr,cb);
    mreq->type       = msg_write;
    mreq->parent_req = 0;
    mreq->dinst = 0;
    return mreq;
  }
  static MemRequest *createWrite(MemObj *m, DInst *dinst, CallbackBase *cb=0) {
    MemRequest *mreq = create(m,dinst->getAddr(),cb);
    mreq->type       = msg_write;
    mreq->parent_req = 0;
    mreq->dinst = dinst; // FERMI
    return mreq;
  }
  static MemRequest *createVPCWriteUpdate(MemObj *m, DInst *dinst, CallbackBase *cb=0) {
    MemRequest *mreq = create(m,dinst->getAddr(),cb);
    mreq->data       = dinst->getData();
    mreq->pc         = dinst->getPC();
    mreq->type       = msg_vpcwriteupdate;
    mreq->parent_req = 0;
    mreq->dinst = dinst; // FERMI
    return mreq;
  }
  static MemRequest *createWriteAddress(MemObj *m, DInst *dinst, CallbackBase *cb=0) {
    MemRequest *mreq = create(m,dinst->getAddr(),cb);
    mreq->type       = msg_writeAddress;
    mreq->parent_req = 0;
    mreq->dinst      = dinst;
    return mreq;
  }

  static MemRequest *createWriteback(MemObj *m, AddrType addr) {
    MemRequest *mreq = create(m,addr,0);
    mreq->type       = msg_writeback;
    mreq->parent_req = 0;
    mreq->dinst      = 0;
    return mreq;
  }

  static MemRequest *createInvalidate(MemObj *m, AddrType naddr, uint16_t size, bool capacityInvalidate) {
    MemRequest *mreq = create(m,naddr,0);
    mreq->type       = msg_invalidate;
    mreq->lineSize   = size;
    mreq->parent_req = 0;
    mreq->pending    = 1;
    mreq->dinst      = 0;
    mreq->capInvalidate = capacityInvalidate;
    return mreq;
  }
  MemRequest *createInvalidate() {
    I(type == msg_invalidate);
    MemRequest *mreq = create(currMemObj,addr,0);
    mreq->type       = msg_invalidate;
    mreq->lineSize   = lineSize;
    mreq->parent_req = 0;
    mreq->dinst      = 0;
    mreq->capInvalidate = capInvalidate;

    pending++;

    return mreq;
  }
  MemRequest *getParent() const { return parent_req; }
#ifdef DEBUG
  uint64_t getid() { return id;}
#endif
  void setNextHop(MemObj *newMemObj);

  void destroy();

  MemObj *getHomeNode() const { return homeMemObj; }
  bool isHomeNode()     const { return homeMemObj == currMemObj; }

  bool hasPending() const { 
    if (pending>0)
      return true;
    // Not a recursive check. Just if the immediate is done
    if (parent_req==0)
      return false; 
    I(parent_req->parent_req==0); // not real recursion for the moment
    return parent_req->pending>0;
  }
  void decPending() {
    I(type == msg_invalidate);
    if (parent_req) {
      I(parent_req->type == msg_invalidate);
      I(parent_req->pending>0);
      parent_req->pending--;
    }else{
//      I(pending>0);
      pending--;
    }
  }

  bool isPrefetch() const {return prefetch; }
  void markPrefetch() {
    I(!prefetch);
    prefetch = true;
  }

  bool isMMU() const {return mmu; }
  void markMMU() {
    I(!mmu);
    mmu = true;
  }

  void ack() {
    if(cb)
      cb->call();
    dump_calledge(0);
    destroy();
  }
  void ack(TimeDelta_t lat) {
    I(lat);

    if(cb) // Not all the request require a completion notification
      cb->schedule(lat);
    dump_calledge(lat);
    destroy();
  }
  void ackAbs(Time_t when) {
    I(when);
    if(cb)
      cb->scheduleAbs(when);
    dump_calledge(when-globalClock);
    destroy();
  }

  Time_t  getTimeDelay()  const { return globalClock-startClock; }

  AddrType getAddr() const { return addr; }
  AddrType getPC()   const { return pc; }
  DataType getData() const { return data; }

  uint16_t getLineSize() const { return lineSize; };

  bool isRead()               const { return type == msg_read;           }
  bool isVPCWriteUpdate()     const { return type == msg_vpcwriteupdate; }
  bool isWrite()              const { return type == msg_write;          }
  bool isWriteAddress()       const { return type == msg_writeAddress;   }
  bool isWriteback()          const { return type == msg_writeback;      }
  bool isInvalidate()         const { return type == msg_invalidate;     }

  void fwdRead(                  )     { 
    upce(0);
    Time_t when = currMemObj->nextReadSlot(this);
    readCB.scheduleAbs(when);
  }
  void fwdRead(TimeDelta_t       lat)  { 
    upce(lat);
    Time_t when = currMemObj->nextReadSlot(this);
    readCB.scheduleAbs(when+lat);
  }
  void fwdWrite(                 )     { 
    upce(0);
    Time_t when = currMemObj->nextWriteSlot(this);
    writeCB.scheduleAbs(when);          
  }
  void fwdWrite(TimeDelta_t      lat)  { 
    upce(lat);     
    Time_t when = currMemObj->nextWriteSlot(this);
    writeCB.scheduleAbs(when+lat);          
  }
  void fwdWriteAddress(                 )     { 
    upce(0);
    Time_t when = currMemObj->nextWriteSlot(this);
    writeAddressCB.scheduleAbs(when);          
  }
  void fwdWriteAddress(TimeDelta_t      lat)  { 
    upce(lat);     
    Time_t when = currMemObj->nextWriteSlot(this);
    writeAddressCB.scheduleAbs(when+lat);          
  }

  void fwdBusRead(               )     { 
    upce(0);
    Time_t when = currMemObj->nextBusReadSlot(this);
    busReadCB.scheduleAbs(when);        
  }
  void fwdBusRead(TimeDelta_t    lat)  { 
    upce(lat);     
    Time_t when = currMemObj->nextBusReadSlot(this);
    busReadCB.scheduleAbs(when+lat);        
  }

  void fwdPushDown(              )     { 
    upce(0);       
    Time_t when = currMemObj->nextPushDownSlot(this);
    pushDownCB.scheduleAbs(when);
  }
  void fwdPushDown(TimeDelta_t   lat)  { 
    upce(lat);     
    Time_t when = currMemObj->nextPushDownSlot(this);
    pushDownCB.scheduleAbs(when+lat);
  }

  void fwdPushUp(                )     { 
    upce(0);       
    Time_t when = currMemObj->nextPushUpSlot(this);
    pushUpCB.scheduleAbs(when);         
  }
  void fwdPushUp(TimeDelta_t     lat)  { 
    upce(lat);     
    Time_t when = currMemObj->nextPushUpSlot(this);
    pushUpCB.scheduleAbs(when+lat);         
  }

  void fwdInvalidate(            )     { 
    upce(0);       
    Time_t when = currMemObj->nextInvalidateSlot(this);
    invalidateCB.scheduleAbs(when);
  }
  void fwdInvalidate(TimeDelta_t lat)  { 
    upce(lat);
    Time_t when = currMemObj->nextInvalidateSlot(this);
    invalidateCB.scheduleAbs(when+lat);
  }

  DInst *getDInst() const       { return dinst; }

#ifdef ENABLE_CUDA
  bool isSharedAddress() const { 
    if (dinst == 0x0) {
      if ((addr >> 61) == 6){
        return true;
      } 
      return false;
    }
    return dinst->isSharedAddress(); 
  } 
#endif

  // UGGLY code for DDR2 
  MemRequest *front;
  MemRequest *back;
  bool loadMiss;
  int sequenceNum;
  int coreID;
  bool isLoadMiss() const {return loadMiss;}
  bool getStatsFlag() const { 

    if (dinst) 
      return dinst->getStatsFlag(); 
    return true; // if not bounded to a DInst, just do the stats
  }
  bool isCapacityInvalidate() { return capInvalidate; }
  
  float getL1clkRatio(){ return dinst->getL1clkRatio(); }
  float getL3clkRatio(){ return dinst->getL3clkRatio(); }

};

class MemRequestHashFunc {
 public: 
  size_t operator()(const MemRequest *mreq) const {
    size_t val = (size_t)mreq;
    return val>>2;
  }
};


#endif   // MEMREQUEST_H
