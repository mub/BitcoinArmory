#ifndef _BLOCKUTILS_H_
#define _BLOCKUTILS_H_

#include <stdio.h>
#include <iostream>
#ifdef WIN32
   #include <cstdint>
#else
   #include <stdlib.h>
   #include <inttypes.h>
   #include <cstring>
#endif
#include <fstream>
#include <vector>
#include <queue>
#include <deque>
#include <list>
#include <map>
#include <limits>

#include "BinaryData.h"
//#include "BinaryDataRef.h"

#include "cryptlib.h"
#include "sha.h"
#include "UniversalTimer.h"


#define HEADER_SIZE       80
#define BinaryData BinaryData

//#define UINT16_SENTINEL numeric_limits<uint16_t>::max()
//#define UINT32_SENTINEL numeric_limits<uint32_t>::max()
//#define UINT64_SENTINEL numeric_limits<uint64_t>::max()




using namespace std;


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// This class doesn't hold actually block header data, it only holds pointers
// to where the data is in the BlockHeaderManager.  So there is a single place
// where all block headers are stored, and this class tells us where exactly
// is the one we want.

class Tx;

class BlockHeader
{

   friend class BlockDataManager;
   friend class BlockHeaderRef;

public: 

   /////////////////////////////////////////////////////////////////////////////
   uint32_t            getVersion(void) const     { return version_;      }
   BinaryData const &  getPrevHash(void) const    { return prevHash_;     }
   BinaryData const &  getMerkleRoot(void) const  { return merkleRoot_;   }
   uint32_t            getTimestamp(void) const   { return timestamp_;    }
   BinaryData const &  getDiffBits(void) const    { return diffBitsRaw_;  }
   uint32_t            getNonce(void) const       { return nonce_;        }
   BinaryData const &  getThisHash(void) const    { return thisHash_;     }
   BinaryData const &  getNextHash(void) const    { return nextHash_;     }
   uint32_t            getNumTx(void) const       { return numTx_;        }
   uint32_t            getBlockHeight(void) const { return blockHeight_;  }
   
   /////////////////////////////////////////////////////////////////////////////
   void setVersion(uint32_t i)        { version_ = i;                    }
   void setPrevHash(BinaryData str)   { prevHash_.copyFrom(str);         }
   void setMerkleRoot(BinaryData str) { merkleRoot_.copyFrom(str);       }
   void setTimestamp(uint32_t i)      { timestamp_ = i;                  }
   void setDiffBits(BinaryData str)   { diffBitsRaw_.copyFrom(str);      }
   void setNonce(uint32_t i)          { nonce_ = i;                      }
   void setNextHash(BinaryData str)   { nextHash_.copyFrom(str);         }

   /////////////////////////////////////////////////////////////////////////////
   void serialize(BinaryWriter & bw)
   {
      bw.put_uint32_t  ( version_     );
      bw.put_BinaryData( prevHash_    );
      bw.put_BinaryData( merkleRoot_  );
      bw.put_uint32_t  ( timestamp_   );
      bw.put_BinaryData( diffBitsRaw_ );
      bw.put_uint32_t  ( nonce_       );
   }

   /////////////////////////////////////////////////////////////////////////////
   BinaryData serialize(void)
   {
      BinaryWriter bw(HEADER_SIZE);
      serialize(bw);
      return bw.getData();
   }

   /////////////////////////////////////////////////////////////////////////////
   void unserialize(BinaryReader & br)
   {
      BinaryData str;
      br.get_BinaryData(str, 80);
      unserialize(str);
   }

   /////////////////////////////////////////////////////////////////////////////
   void unserialize(BinaryData const & str)
   {
      unserialize(str.getPtr());
   } 

   /////////////////////////////////////////////////////////////////////////////
   void unserialize(BinaryDataRef const & str)
   {
      unserialize(str.getPtr());
   } 

   void unserialize(uint8_t const * start)
   {
      version_ = *(uint32_t*)(   start +  0     );
      prevHash_.copyFrom(        start +  4, 32 );
      merkleRoot_.copyFrom(      start + 36, 32 );
      timestamp_ = *(uint32_t*)( start + 68     );
      diffBitsRaw_.copyFrom(     start + 72, 4  );
      nonce_ = *(uint32_t*)(     start + 76     );
   }

   /////////////////////////////////////////////////////////////////////////////
   BlockHeader( uint8_t const * bhDataPtr,
                BinaryData* thisHash = NULL) :
      prevHash_(32),
      nextHash_(32),
      numTx_(-1),
      fileByteLoc_(0),  
      difficultyFlt_(-1.0),
      difficultySum_(-1.0),
      blockHeight_(0),
      isMainBranch_(false),
      isOrphan_(false),
      isFinishedCalc_(false)
   {
      unserialize(bhDataPtr);
      if( thisHash != NULL )
         thisHash_.copyFrom(*thisHash);
      else
         BtcUtils::getHash256(bhDataPtr, HEADER_SIZE, thisHash_);
   }

   /////////////////////////////////////////////////////////////////////////////
   BlockHeader( BinaryData const * serHeader = NULL,
                BinaryData const * suppliedHash  = NULL,
                uint64_t           fileLoc   = UINT64_MAX) :
      prevHash_(32),
      thisHash_(32),
      nextHash_(32),
      numTx_(-1),
      fileByteLoc_(fileLoc),  
      difficultyFlt_(-1.0),
      difficultySum_(-1.0),
      blockHeight_(0),
      isMainBranch_(false),
      isOrphan_(false),
      isFinishedCalc_(false),
      isOnDiskYet_(false)
   {
      if(serHeader != NULL)
      {
         unserialize(*serHeader);
         if( suppliedHash != NULL )
            thisHash_.copyFrom(*suppliedHash);
         else
            BtcUtils::getHash256(*serHeader, thisHash_);
      }
   }


   /////////////////////////////////////////////////////////////////////////////
   static double convertDiffBitsToDouble(uint32_t diffBits)
   {
       int nShift = (diffBits >> 24) & 0xff;
       double dDiff = (double)0x0000ffff / (double)(diffBits & 0x00ffffff);
   
       while (nShift < 29)
       {
           dDiff *= 256.0;
           nShift++;
       }
       while (nShift > 29)
       {
           dDiff /= 256.0;
           nShift--;
       }
       return dDiff;
   }

   /////////////////////////////////////////////////////////////////////////////
   void printBlockHeader(ostream & os=cout)
   {
      os << "Block Information: " << blockHeight_ << endl;
      os << " Hash:       " << thisHash_.toHex().c_str() << endl;
      os << " Timestamp:  " << getTimestamp() << endl;
      os << " Prev Hash:  " << prevHash_.toHex().c_str() << endl;
      os << " MerkleRoot: " << getMerkleRoot().toHex().c_str() << endl;
      os << " Difficulty: " << (uint64_t)(difficultyFlt_)
                             << "    (" << getDiffBits().toHex().c_str() << ")" << endl;
      os << " CumulDiff:  " << (uint64_t)(difficultySum_) << endl;
      os << " Nonce:      " << getNonce() << endl;
      os << " FileOffset: " << fileByteLoc_ << endl;
   }


private:

   // All these pointers point to data managed by another class.
   // As such, it is unnecessary to deal with any memory mgmt. 

   // Some more data types to be stored with the header, but not
   // part of the official serialized header data, so these are
   // actual members of the BlockHeader.
   uint32_t       version_;
   BinaryData     prevHash_;
   BinaryData     merkleRoot_;
   uint32_t       timestamp_;
   BinaryData     diffBitsRaw_; 
   uint32_t       nonce_; 

   BinaryData     thisHash_;
   BinaryData     nextHash_;
   uint32_t       numTx_;
   uint32_t       blockHeight_;
   uint64_t       fileByteLoc_;
   double         difficultyFlt_;
   double         difficultySum_;
   bool           isMainBranch_;
   bool           isOrphan_;
   bool           isFinishedCalc_;
   bool           isOnDiskYet_;

   vector<Tx*>    txPtrList_;

};


class BlockHeaderRef
{
   friend class BlockDataManager;

public:

   BlockHeaderRef(uint8_t const * ptr)
   {
      self_.setRef(ptr, HEADER_SIZE);
   }

   uint32_t      getVersion(void) const     { return *(uint32_t*)self_;         }
   BinaryDataRef getPrevHash(void) const    { return BinaryDataRef(self_+4,32); }
   BinaryDataRef getMerkleRoot(void) const  { return BinaryDataRef(self_+36,32);}
   uint32_t      getTimestamp(void) const   { return *(uint32_t*)(self+68);     }
   BinaryDataRef getDiffBits(void) const    { return BinaryDataRef(self_+72,4); }
   uint32_t      getNonce(void) const       { return *(uint32_t*)(self_+76);    }

   uint8_t const * getPtr(void) { return self_.getPtr(); }
   uint8_t const * getBinaryDataRef(void) { return self_; }
   uint32_t        getSize(void) { return self_.getSize(); }

   BlockHeader getCopy(void)
   {
      BlockHeader bh;
      bh.unserialize(self_);

      bh.thisHash_     = thisHash_;
      bh.nextHash_     = nextHash_;
      bh.numTx_        = numTx_;
      bh.blockHeight_  = blockHeight_;
      bh.fileByteLoc_  = fileByteLoc_;
      bh.difficultyFlt_ = difficultyFlt_;
      bh.difficultySum_ = difficultySum_;
      bh.isMainBranch_ = isMainBranch_;
      bh.isOrphan_     = isOrphan_;
      bh.isFinishedCalc_ = isFinishedCalc_;
      bh.isOnDiskYet_  = isOnDiskYet_;
      bh.txPtrList_    = txPtrList_;

      return bh;
   }

private:
   BinaryDataRef  self_;

   BinaryData     thisHash_;
   BinaryData     nextHash_;
   uint32_t       numTx_;
   uint32_t       blockHeight_;
   uint64_t       fileByteLoc_;
   double         difficultyFlt_;
   double         difficultySum_;
   bool           isMainBranch_;
   bool           isOrphan_;
   bool           isFinishedCalc_;
   bool           isOnDiskYet_;
   vector<Tx*>    txPtrList_;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// OutPoint is just a reference to a TxOut
class OutPoint
{
   friend class BlockDataManager;
   friend class OutPointRef;

public:
   OutPoint(void) :
      txHash_(32),
      txOutIndex_(UINT32_MAX)
   {
      // Nothing to put here
   }

   OutPoint(BinaryData & txHash, uint32_t txOutIndex) :
      txHash_(txHash),
      txOutIndex_(txOutIndex)
   {
      // Nothing to put here
   }

   BinaryData const & getTxHash(void) { return txHash_; }
   uint32_t getTxOutIndex(void) { return txOutIndex_; }

   void setTxHash(BinaryData const & hash) { txHash_.copyFrom(hash); }
   void setTxOutIndex(uint32_t idx) { txOutIndex_ = idx; }

   // Define these operators so that we can use OutPoint as a map<> key
   bool operator<(OutPoint const & op2) const
   {
      if(txHash_ == op2.txHash_)
         return txOutIndex_ < op2.txOutIndex_;
      else
         return txHash_ < op2.txHash_;
   }
   bool operator==(OutPoint const & op2) const
   {
      return (txHash_ == op2.txHash_ && txOutIndex_ == op2.txOutIndex_);
   }

   void serialize(BinaryWriter & bw)
   {
      bw.put_BinaryData(txHash_);
      bw.put_uint32_t(txOutIndex_);
   }

   BinaryData serialize(void)
   {
      BinaryWriter bw(36);
      serialize(bw);
      return bw.getData();
   }

   void unserialize(uint8_t const * ptr)
   {
      txHash_.copyFrom(ptr, 32);
      txOutIndex_ = *(uint32_t*)(ptr+32);
   }
   void unserialize(BinaryData const & bd)
   {
      unserialize(bd.getPtr());
   }
   void unserialize(BinaryDataRef const & bdRef)
   {
      unserialize(bdRef.getPtr());
   }
   void unserialize(BinaryReader & br)
   {
      br.get_BinaryData(txHash_, 32);
      txOutIndex_ = br.get_uint32_t();
   }
   void unserialize(BinaryRefReader & brr)
   {
      brr.get_BinaryData(txHash_, 32);
      txOutIndex_ = brr.get_uint32_t();
   }

   void unserialize(BinaryData    const & str) { unserialize(str.getPtr()); }
   void unserialize(BinaryDataRef const & str) { unserialize(str.getPtr()); }

private:
   BinaryData txHash_;
   uint32_t   txOutIndex_;

};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// OutPoint is just a reference to a TxOut
class OutPointRef
{
   friend class BlockDataManager;

public:
   OutPointRef(uint8_t const * ptr)
   {
      self_.setRef(ptr, 36);
   }

   uint8_t const * getPtr(void) { return self_.getPtr(); }
   uint8_t const * getBinaryDataRef(void) { return self_; }
   uint32_t        getSize(void) { return self_.getSize(); }

   OutPoint getCopy(void) 
   {
      OutPoint op;
      op.unserialize( self_.getPtr() );
      return op;
   }

   BinaryData    getTxHash(void)     { return BinaryData(self_.getPtr(), 32); }
   BinaryDataRef getTxHashRef(void)  { return BinaryDataRef(self_.getPtr(),32); }
   uint32_t      getTxOutIndex(void) { return *(uint32_t*)(self_.getPtr()+32); }


   void unserialize(BinaryRefReader & br)
   {
      self_ = br.get_BinaryDataRef(36);
   }

   void unserialize(BinaryDataRef const & str)
   {
      self_.setRef(str.getPtr(), 36);
   }

   BinaryDataRef serialize(void)
   {
      return self_;
   }


private:
   BinaryDataRef self_;

};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TxIn 
class TxIn
{
   friend class BlockDataManager;
   friend class TxInRef;

public:
   TxIn(void) :
      outPoint_(),
      binScript_(0),
      sequence_(0),
      isCoinbase_(false),
      scriptSize_(0)
   {
      // Nothing to put here
   }

   TxIn(OutPoint const & op,
        BinaryData const & script,
        uint32_t seq,
        bool coinbase) : 
      outPoint_(op),
      binScript_(script),
      sequence_(seq),
      isCoinbase_(coinbase)
   {
      scriptSize_ = (uint32_t)(binScript_.getSize());
   }

   OutPoint const & getOutPoint(void) { return outPoint_; }
   BinaryData const & getBinScript(void) { return binScript_; }
   uint32_t getSequence(void) { return sequence_; }
   uint32_t getScriptSize(void) { return scriptSize_; }
   uint32_t getSize(void) { return nBytes_; }

   bool getIsCoinbase(void) 
   { 
      // TODO: figure out if this is a coinbase TxIn
      return isCoinbase_;
   }

   void setOutPoint(OutPoint const & op) { outPoint_ = op; }
   void setBinScript(BinaryData const & scr) { binScript_.copyFrom(scr); }
   void setSequence(uint32_t seq) { sequence_ = seq; }
   void setIsCoinbase(bool iscb) { isCoinbase_ = iscb; }
   void setIsMine(bool ismine) { isMine_ = ismine; }


   void serialize(BinaryWriter & bw)
   {
      outPoint_.serialize(bw);
      bw.put_var_int(scriptSize_);
      bw.put_BinaryData(binScript_);
      bw.put_uint32_t(sequence_);
   }

   BinaryData serialize(void)
   {
      BinaryWriter bw(250);
      serialize(bw);
      return bw.getData();
   }

   void unserialize(uint8_t const * ptr)
   {
      outPoint_.unserialize(ptr);
      uint32_t viLen;
      scriptSize_ = BtcUtils::readVarInt(ptr+36, &viLen);
      binScript_.copyFrom(ptr+36+viLen, scriptSize_);
      sequence_ = *(uint32_t*)(ptr+36+viLen+scriptSize_);
      nBytes_ = 36+viLen+scriptSize_+4;
   }
   void unserialize(BinaryReader & br)
   {
      uint32_t posStart = br.getPosition();
      outPoint_.unserialize(br);
      scriptSize_ = (uint32_t)br.get_var_int();
      br.get_BinaryData(binScript_, scriptSize_);
      sequence_ = br.get_uint32_t();

      nBytes_ = br.getPosition() - posStart;
   }
   void unserialize(BinaryRefReader & brr)
   {
      uint32_t posStart = brr.getPosition();
      outPoint_.unserialize(brr);
      scriptSize_ = (uint32_t)brr.get_var_int();
      brr.get_BinaryData(binScript_, scriptSize_);
      sequence_ = brr.get_uint32_t();
      nBytes_ = br.getPosition() - posStart;
   }

   void unserialize(BinaryData const & str)
   {
      unserialize(str.getPtr());
   }
   void unserialize(BinaryDataRef const & str)
   {
      unserialize(str.getPtr());
   }


private:
   OutPoint   outPoint_;
   uint32_t   scriptSize_;
   BinaryData binScript_;
   uint32_t   sequence_;

   uint32_t   nBytes_;

   bool       isCoinbase_;
   bool       isMine_;

};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class TxInRef
{
   friend class BlockDataManager;

public:
   TxInRef(uint8_t const * ptr, uint32_t nBytes=0)
   {
      setRef(ptr, nBytes);
   }

   void setRef(uint8_t const * ptr, uint32_t nBytes=0)
   {
      if(nbytes==0)
         nbytes = BtcUtils::TxInCalcLength(ptr);
      self.setRef(ptr, nBytes);
   }

   uint8_t const * getPtr(void) { return self_.getPtr(); }
   uint8_t const * getBinaryDataRef(void) { return self_; }
   uint32_t        getSize(void) { return self_.getSize(); }


   TxIn getCopy(void)
   {
      TxIn returnTxIn;
      returnTxIn.unserialize(getPtr());
      returnTxIn.isCoinbase_ = isCoinbase_;
      returnTxIn.isMine_ = isMine_;
      return returnTxIn;
   }

   uint32_t getScriptOffset(void)
   {
      return 36 + BtcUtils::readVarIntLength(getPtr()+36);
   }

   OutPoint getOutPoint(void) 
   { 
      OutPoint op;
      op.unserialize(getPtr());
      return op;
   }
   OutPointRef getOutPointRef(void) 
   { 
      return OutPointRef(getPtr());
   }
   BinaryData getBinScript(void) 
   { 
      uint32_t scrLen = BtcUtils::readVarInt(getPtr()+36);
      return BinaryData(getPtr() + getScriptOffset(), scrLen);
   }
   BinaryDataRef getBinScriptRef(void) 
   { 
      uint32_t scrLen = BtcUtils::readVarInt(getPtr()+36);
      return BinaryDataRef(getPtr() + getScriptOffset(), scrLen);
   }
   uint32_t getSequence(void)   { return *(uint32_t*)(getPtr() + getSize() - 4); }
   uint32_t getScriptSize(void) { return BtcUtils::readVarInt(getPtr()+36); }


   void unserialize(BinaryRefReader & brr, uint32_t nbytes=0)
   {
      if(nbytes==0)
         nbytes = BtcUtils::TxInCalcLength(brr.getPosPtr());
      self_ = brr.get_BinaryDataRef(nbytes);
   }

   void unserialize(BinaryDataRef const & str, uint32_t nbytes=0)
   {
      if(nbytes==0)
         nbytes = BtcUtils::TxInCalcLength(brr.getPosPtr());
      self_.setRef(str.getPtr(), nbytes);
   }

   BinaryDataRef serialize(void) 
   { 
      return self_;
   }


private:
   BinaryDataRef self_;

   bool       isCoinbase_;
   bool       isMine_;

};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TxOut 
class TxOut
{
   friend class BlockDataManager;
   friend class TxOutRef;

public:
   TxOut(void) :
      value_(0),
      pkScript_(0),
      scriptSize_(0)
   {
      // Nothing to put here
   }

   TxOut(uint64_t val, BinaryData const & scr) :
      value_(val),
      pkScript_(scr)
   {
      scriptSize_ = (uint32_t)(pkScript_.getSize());
   }

   uint64_t getValue(void) { return value_; }
   BinaryData const & getPkScript(void) { return pkScript_; }
   uint32_t getScriptSize(void) { return scriptSize_; }

   void setValue(uint64_t val) { value_ = val; }
   void setPkScript(BinaryData const & scr) { pkScript_.copyFrom(scr); }


   bool isStandardScript(void) const
   {
      return ((pkScript_[0            ] == 118 &&
               pkScript_[1            ] == 169 &&
               pkScript_[scriptSize_-2] == 136 &&
               pkScript_[scriptSize_-1] == 172   ) ||

               // TODO: I'm pretty sure (LenPK + 0x04 + PUBKEY + OP_CHECKSIG)
              (pkScript_[scriptSize_-1] == 172 && 
               scriptSize_ == 67)                     )
   }

   BinaryData const & getRecipientAddr(void)
   {
      if( !isStandardScript() )
         return BtcUtils::badAddress_;

      if(recipientAddr_.getSize() < 1)
      {
         BinaryReader binReader(pkScript_);
         binReader.advance(2);
         uint64_t addrLength = (uint32_t)binReader.get_var_int();
         recipientAddr_.resize((size_t)addrLength);
         binReader.get_BinaryData(recipientAddr_, (uint32_t)addrLength);
      }

      return recipientAddr_;
   }



   void unserialize(uint8_t const * ptr)
   {
      value_ = *(uint64_t*)(ptr);
      uint32_t viLen;
      scriptSize_ = BtcUtils::readVarInt(ptr+8, &viLen);
      pkScript_.copyFrom(ptr+8+viLen, scriptSize_);
      nBytes_ = 8 + viLen + scriptSize_;
   }
   void unserialize(BinaryReader & br)
   {
      uint32_t posStart = br.getPosition();
      value_ = br.get_uint64_t();
      scriptSize_ = (uint32_t)br.get_var_int();
      br.get_BinaryData(pkScript_, scriptSize_);
      nBytes_ = br.getPosition() - posStart;
   }

   void unserialize(BinaryRefReader & brr)
   {
      uint32_t posStart = brr.getPosition();
      value_ = brr.get_uint64_t();
      scriptSize_ = (uint32_t)brr.get_var_int();
      brr.get_BinaryData(pkScript_, scriptSize_);
      nBytes_ = brr.getPosition() - posStart;
   }

   void unserialize(BinaryData const & str)
   {
      unserialize(str.getPtr());
   }
   void unserialize(BinaryDataRef const & str)
   {
      unserialize(str.getPtr());
   }


   void serialize(BinaryWriter & bw)
   {
      bw.put_uint64_t(value_);
      bw.put_var_int(scriptSize_);
      bw.put_BinaryData(pkScript_);
   }

   BinaryData serialize(void)
   {
      BinaryWriter bw(45);
      serialize(bw);
      return bw.getData();
   }
private:
   uint64_t   value_;
   uint32_t   scriptSize_;
   BinaryData pkScript_;

   uint32_t   nBytes_;
   BinaryData recipientAddr_;
   bool       isMine_;
   bool       isSpent_;

};


class TxOutRef
{
   friend class BlockDataManager;

public:

   TxOutRef(uint8_t const * ptr, uint32_t nBytes=0)
   {
      setRef(ptr, nBytes);
   }

   void setRef(uint8_t const * ptr, uint32_t nBytes=0)
   {
      if(nBytes==0)
         nBytes = BtcUtils::TxOutCalcLength(ptr);
      self_.setRef(ptr, nBytes);
   }

   uint8_t const * getPtr(void) { return self_.getPtr(); }
   uint8_t const * getBinaryDataRef(void) { return self_; }
   uint32_t        getSize(void) { return self_.getSize(); }


   TxOut getCopy(void)
   {
      TxOut returnTxOut;
      returnTxOut.unserialize(getPtr());
      returnTxOut.isMine_ = isMine_;
      returnTxOut.isSpent_ = isSpent_;
      returnTxOut.recipientAddr_ = recipientAddr_;
      return returnTxOut;
   }


   void unserialize(BinaryRefReader & brr, uint32_t nBytes=0)
   {
      if(nBytes==0)
         nBytes = BtcUtils::TxOutCalcLength(brr.getPosPtr());
      self_ = brr.get_BinaryDataRef(nBytes);
   }

   void unserialize(BinaryDataRef const & str, uint32_t nBytes=0)
   {
      if(nBytes==0)
         nBytes = BtcUtils::TxOutCalcLength(str.getPtr());
      self_.setRef(str.getPtr(), nBytes);
   }

   BinaryDataRef serialize(void) 
   { 
      return self_;
   }

private:
   BinaryDataRef self_;

   bool       isMine_;
   bool       isSpent_;
   BinaryData recipientAddr_;


};



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class Tx
{
   friend class BlockDataManager;
   friend class TxRef;

public:
   Tx(void) :
      version_(0),
      numTxIn_(0),
      numTxOut_(0),
      txInList_(0),
      txOutList_(0),
      lockTime_(UINT32_MAX),
      thisHash_(32),
      headerPtr_(NULL)
   {
      // Nothing to put here
   }

     
   OutPoint createOutPoint(int txOutIndex)
   {
      return OutPoint(thisHash_, txOutIndex);
   }


   void serialize(BinaryWriter & bw)
   {
      bw.put_uint32_t(version_);
      bw.put_var_int(numTxIn_);
      for(uint32_t i=0; i<numTxIn_; i++)
      {
         txInList_[i].serialize(bw);
      }
      bw.put_var_int(numTxOut_);
      for(uint32_t i=0; i<numTxOut_; i++)
      {
         txOutList_[i].serialize(bw);
      }
      bw.put_uint32_t(lockTime_);
   }

   BinaryData serialize(void)
   {
      BinaryWriter bw(300);
      serialize(bw);
      return bw.getData();
   }

   void unserialize(uint8_t const * ptr)
   {
      // This would be too annoying to write in raw pointer-arithmatic
      // So I will just create a BinaryRefReader
      unserialize(BinaryRefReader(ptr));
   }

   void unserialize(BinaryReader & br)
   {
      uint32_t posStart = br.getPosition();
      version_ = br.get_uint32_t();

      numTxIn_ = (uint32_t)br.get_var_int();
      txInList_ = vector<TxIn>(numTxIn_);
      for(uint32_t i=0; i<numTxIn_; i++)
         txInList_[i].unserialize(br); 

      numTxOut_ = (uint32_t)br.get_var_int();
      txOutList_ = vector<TxOut>(numTxOut_);
      for(uint32_t i=0; i<numTxOut_; i++)
         txOutList_[i].unserialize(br); 

      lockTime_ = br.get_uint32_t();
      nBytes_ = br.getPosition() - posStart;
   }

   void unserialize(BinaryRefReader & brr)
   {
      uint32_t posStart = brr.getPosition();
      version_ = brr.get_uint32_t();

      numTxIn_ = (uint32_t)brr.get_var_int();
      txInList_ = vector<TxIn>(numTxIn_);
      for(uint32_t i=0; i<numTxIn_; i++)
         txInList_[i].unserialize(brr); 

      numTxOut_ = (uint32_t)brr.get_var_int();
      txOutList_ = vector<TxOut>(numTxOut_);
      for(uint32_t i=0; i<numTxOut_; i++)
         txOutList_[i].unserialize(brr); 

      lockTime_ = brr.get_uint32_t();
      nBytes_ = brr.getPosition() - posStart;
   }

   void unserialize(BinaryData const & str)
   {
      unserialize(str.getPtr());
   }
   void unserialize(BinaryDataRef const & str)
   {
      unserialize(str.getPtr());
   }


private:
   uint32_t      version_;
   uint32_t      numTxIn_;
   uint32_t      numTxOut_;
   vector<TxIn>  txInList_;
   vector<TxOut> txOutList_;
   uint32_t      lockTime_;
   
   BinaryData    thisHash_;
   uint32_t      nBytes_;
   BlockHeader*  headerPtr_;
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class TxRef
{
   friend class BlockDataManager;

public:
   TxRef(uint8_t const * ptr)
   {
      setRefCalcSize(ptr)
   }

   // TODO: Check whether we even need a shortcut algorithm, we might just hurt
   //       ourself in the long run by having to check a flag everytime we try
   //       to get a TxIn/TxOut
   //TxRef(uint8_t const * ptr, uint32_t nBytes)
   //{
      //setRef(ptr, nBytes);
   //}

   void setRefCalcSize(uint8_t const * ptr)
   {
      nBytes_ = BtcUtils::TxCalcLength(ptr, &offsetsTxIn_, &offsetsTxOut_);
      self_.setRef(ptr, nBytes_);
   }

   //void setRef(uint8_t const * ptr, uint32_t nBytes)
   //{
      //isOffsetsCalc_ = false;
      //self_.setRef(ptr, nBytes);
   //}


     
   uint8_t const * getPtr(void) { return self_.getPtr(); }
   uint8_t const * getBinaryDataRef(void) { return self_; }
   uint32_t        getSize(void) { return self_.getSize(); }

   uint32_t  getNumTxIn(void)  { return (uint32_t)self_.offsetsTxIn_.size()-1;}
   uint32_t  getNumTxOut(void) { return (uint32_t)self_.offsetsTxOut_.size()-1;}

   BlockHeaderRef  getHeaderPtr_(void)  { return headerPtr_; }

   Tx getCopy(void)
   {
      Tx returnTx;
      returnTx.unserialize(getPtr());
      returnTx.thisHash_ = thisHash_;
      returnTx.nBytes_ = nBytes_;
      returnTx.headerPtr_ = headerPtr_;
      return returnTx;
   }


   void unserialize(BinaryRefReader & brr)
   {
      setRefCalcSize(brr.getPosPtr());
      brr.advance(nBytes_);
   }

   void unserialize(BinaryDataRef const & str)
   {
      setRef(str.getPtr());
   }

   void unserialize(BinaryData const & str)
   {
      setRef(str.getPtr());
   }

   BinaryDataRef serialize(void) 
   { 
      return self_;
   }

   BinaryData getHash(void)
   {
      return BtcUtils::getHash256(thisPtr, nBytes_, thisHash_);
   }

   BinaryDataRef getHashRef(void) const
   {
      return BinaryDataRef(thisHash_);
   }

   //    
   //void calcOffsets(void)
   //{
      //nBytes_ = BtcUtils::TxCalcLength(ptr, &offsetsTxIn_, &offsetsTxOut_);
   //}

   TxInRef getTxInRef(int i)
   {
      uint32_t txinSize = offsetsTxIn_[i+1] - offsetsTxIn_[i];
      return TxInRef(self.getPtr()+offsetsTxIn_[i], txinSize);
   }

   TxOutRef getTxOutRef(int i)
   {
      uint32_t txoutSize = offsetsTxOut_[i+1] - offsetsTxOut_[i];
      return TxInRef(self.getPtr()+offsetsTxOut_[i], txoutSize);
   }

   TxIn  getTxIn (int i) { return getTxInRef (i).getCopy(); }
   TxOut getTxOut(int i) { return getTxOutRef(i).getCopy(); }



private:
   BinaryDataRef self_; 

   BinaryData    thisHash_;
   uint32_t      nBytes_;
   BlockHeader*  headerPtr_;

   // A transaction is just a big blob of bits with variable-length fields.
   // We don't actually know where each TxIn and TxOut starts without scanning
   // the blob.  
   //bool             isOffsetsCalc_;
   vector<uint32_t> offsetsTxIn_;
   vector<uint32_t> offsetsTxOut_;

};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// The goal of this class is to create a memory pool in RAM that looks exactly
// the same as the block-headers storage on disk.  There is no serialization
// or unserialization, we just copy back and forth between disk and RAM, and 
// we're done.  So it should be about as fast as theoretically possible, you 
// are limited only by your disk I/O speed.
//
// This is more of a simple test, which will later be applied to the entire
// blockchain.  If it works as expected, then this will potentially be useful
// for the official BTC client which seems to have some speed problems at 
// startup and shutdown.
//
// This class is a singleton -- there can only ever be one, accessed through
// the static method GetInstance().  This method gets the single instantiation
// of the BDM class, and then its public members can be used to access the 
// block data that is sitting in memory.
//
class BlockDataManager
{
private:

   BinaryData  blockFileData_ALL_;
   BinaryData  blockFileData_NEW_;
   
   map<BinaryData, BlockHeaderRef>      headerMap_;
   deque<BlockHeaderRef*>               headersByHeight_;
   BlockHeader*                         topBlockPtr_;
   BlockHeader*                         genBlockPtr_;
   map<BinaryData, Tx>                  txMap_;



   // The following two deques should be parallel
   map<BinaryData, BinaryData>       myAccounts_;  
   uint64_t                          myBalance_;

   map<OutPoint, TxOut*>   myTxOuts_;
   map<OutPoint, TxOut*>   myUnspentTxOuts_;
   map<OutPoint, TxOut*>   pendingTxOuts_;
   map<OutPoint, TxOut*>   myTxOutsNonStandard_;
   map<OutPoint, TxIn*>    myTxIns_;



   /*
   typedef map<BinaryData, BlockHeader>::iterator HeadMapIter;
   typedef map<BinaryData, Tx         >::iterator TxMapIter;

   static BlockDataManager *         theOnlyBDM_;

   map<BinaryData, BlockHeader>      headerMap_;
   deque<BlockHeader*>               headersByHeight_;
   BlockHeader*                      topBlockPtr_;
   BlockHeader*                      genBlockPtr_;
   map<BinaryData, Tx>               txMap_;



   // The following two deques should be parallel
   map<BinaryData, BinaryData>       myAccounts_;  
   uint64_t                          myBalance_;

   map<OutPoint, TxOut*>   myTxOuts_;
   map<OutPoint, TxOut*>   myUnspentTxOuts_;
   map<OutPoint, TxIn*>    myTxIns_;

   map<OutPoint, TxOut*>   myTxOutsNonStandard_;
   */


private:
   // Set the constructor to private so that only one can ever be created
   BlockDataManager(void) : 
         topBlockPtr_(NULL),
         genBlockPtr_(NULL),
         myBalance_(0)
   {
      headerMap_.clear();
      txMap_.clear();
      myAccounts_.clear();
      headersByHeight_.clear();
      myTxOutsNonStandard_.clear();
   }

public:

   /////////////////////////////////////////////////////////////////////////////
   // The only way to "create" a BDM is with this method, which creates it
   // if one doesn't exist yet, or returns a reference to the only one
   // that will ever exist
   static BlockDataManager & GetInstance(void) 
   {
      static bool bdmCreatedYet_ = false;
      if( !bdmCreatedYet_ )
      {
         theOnlyBDM_ = new BlockDataManager;
         bdmCreatedYet_ = true;
      }
      return (*theOnlyBDM_);
   }


   /////////////////////////////////////////////////////////////////////////////
   BlockHeader getTopBlock(void)
   {
      return *topBlockPtr_;
   }

   /////////////////////////////////////////////////////////////////////////////
   BlockHeader & getGenesisBlock(void)
   {
      if(genBlockPtr_ == NULL)
         genBlockPtr_ = &(headerMap_[BlockHeader::GenesisHash_]);
      return *genBlockPtr_;
   }

   /////////////////////////////////////////////////////////////////////////////
   // Get a blockheader based on its height on the main chain
   BlockHeader & getHeaderByHeight(int index)
   {
      if( index>=0 && index<(int)headersByHeight_.size())
         return *headersByHeight_[index];
   }

   /////////////////////////////////////////////////////////////////////////////
   // The most common access method is to get a block by its hash
   BlockHeader * getHeaderByHash(BinaryData const & blkHash)
   {
      map<BinaryData, BlockHeader>::iterator it = headerMap_.find(blkHash);
      if(it==headerMap_.end())
         return NULL;
      else
         return &(it->second);
   }

   /////////////////////////////////////////////////////////////////////////////
   void addAccount(BinaryData const & addr, BinaryData const & pubKey64B)
   {
      myAccounts_[addr] = pubKey64B;
   }

   /////////////////////////////////////////////////////////////////////////////
   void flagMyTransactions(void)
   {
      if(myAccounts_.size() == 0)
         return;

      map<BinaryData, Tx>::iterator         txIter;
      map<BinaryData, BinaryData>::iterator addrIter;

     
      TIMER_START("ScanTxOuts");
      // We need to accumulate all TxOuts first
      for( txIter  = txMap_.begin();
           txIter != txMap_.end();
           txIter++)
      {
         Tx & thisTx = txIter->second;
         for( addrIter  = myAccounts_.begin();
              addrIter != myAccounts_.end();
              addrIter++)
         {
            for(uint32_t i=0; i<thisTx.numTxOut_; i++)
            {
               if( thisTx.txOutList_[i].pkScript_.contains(addrIter->first))
               {
                  OutPoint op;
                  op.setTxHash(thisTx.thisHash_);
                  op.setTxOutIndex(i);

                  // Already processed this one, before
                  if( !thisTx.txOutList_[i].isMine_ )
                     continue;

                  if( !thisTx.txOutList_[i].isStandardScript() )
                  {
                     cout << "Non-standard script! "  << endl;
                     cout << "\tTx:       " << thisTx.thisHash_.toHex().c_str() << endl;
                     cout << "\tOutIndex: " << i << endl;
                     myTxOutsNonStandard_[op] = &(thisTx.txOutList_[i]);
                  }
                  else
                  {
                     // We use a map indexed by OutPoint so we can easily iterate
                     // OR find a record to delete it easily from the TxIn record
                     TxOut & txout = thisTx.txOutList_[i];
                     txout.isMine_ = true;
                     txout.isSpent_ = false;
                     myTxOuts_[op]        = &txout;
                     myUnspentTxOuts_[op] = &txout;
                     myBalance_ += txout.value_;
                  }
                  
               }
            }
         }
      }
      TIMER_STOP("ScanTxOuts");

      TIMER_START("ScanTxIns");
      // Next we find all the TxIns and delete TxOuts
      for( txIter  = txMap_.begin();
           txIter != txMap_.end();
           txIter++)
      {
         Tx & thisTx = txIter->second;
         for( addrIter  = myAccounts_.begin();
              addrIter != myAccounts_.end();
              addrIter++)
         {
            for(uint32_t i=0; i<thisTx.numTxIn_; i++)
            {
               // can start searching for pub key 64 bytes into binScript
               if( thisTx.txInList_[i].binScript_.contains(addrIter->second, 64))
               {
                  OutPoint const & op = thisTx.txInList_[i].outPoint_;
                  TxIn  & txin  = thisTx.txInList_[i];
                  TxOut & txout = thisTx.txOutList_[op.txOutIndex_];
                  myTxIns_[op] = &(txin);
                  myTxOuts_[op]->isSpent_ = true;
                  myUnspentTxOuts_.erase(op);
                  myBalance_ += txout.value_;
               }
            }
         }
      }
      TIMER_STOP("ScanTxIns");

      cout << "Completed full scan of TxOuts (" << TIMER_READ_SEC("ScanTxOuts")
           << " sec) and TxIns (" << TIMER_READ_SEC("ScanTxIns") << " sec)" << endl;
      
      cout << "TxOuts: " << endl;
      map<OutPoint, TxOut*>::iterator outIter;
      for( outIter  = myTxOuts_.begin();
           outIter != myTxOuts_.begin();
           outIter++)
      {
         OutPoint const & outpt = outIter->first;
         TxOut    & txout = *(outIter->second);
         cout << "\t" << outpt.txHash_.toHex().c_str();
         cout << "(" << outpt.txOutIndex_ << ")";
         if(txout.isSpent_)
            cout << "\t(SPENT)";
         cout << endl;
      }

      cout << "TxIns: " << endl;
      map<OutPoint, TxIn*>::iterator inIter;
      for( inIter  = myTxIns_.begin();
           inIter != myTxIns_.begin();
           inIter++)
      {
         OutPoint const & outpt = inIter->first;
         TxIn     & txin  = *(inIter->second);
         cout << "\t" << outpt.txHash_.toHex().c_str();
         cout << "(" << outpt.txOutIndex_ << ")";
         cout << endl;
      }
   }

   /////////////////////////////////////////////////////////////////////////////
   // Add headers from a file that is serialized identically to the way
   // we have laid it out in memory.  Return the number of bytes read
   uint64_t importHeadersFromHeaderFile(std::string filename)
   {
      cout << "Reading block headers from file: " << filename.c_str() << endl;
      ifstream is(filename.c_str(), ios::in | ios::binary);
      is.seekg(0, ios::end);
      size_t filesize = (size_t)is.tellg();
      is.seekg(0, ios::beg);
      cout << filename.c_str() << " is " << filesize << " bytes" << endl;
      if((unsigned int)filesize % HEADER_SIZE != 0)
      {
         cout << "filesize=" << filesize << " is not a multiple of header size!" << endl;
         return -1;
      }
      BinaryData allHeaders(filesize);
      uint8_t* front = allHeaders.getPtr();
      is.read((char*)front, filesize);
      BinaryData thisHash(32);
      for(int offset=0; offset<(int)filesize; offset+=HEADER_SIZE)
      {
         uint8_t* thisPtr = front + offset;
         BtcUtils::getHash256(thisPtr, HEADER_SIZE, thisHash);
         headerMap_[thisHash] = BlockHeader(thisPtr, &thisHash);
      }
      cout << "Done with everything!  " << filesize << " bytes read!" << endl;
      return filesize;      
   }


   /*
   /////////////////////////////////////////////////////////////////////////////
   uint32_t importFromBlockFile(std::string filename, bool justHeaders=false)
   {
      // TODO: My original everythingRef solution for headers would save a LOT
      //       of computation here.  At 140k blocks, with 1.1 million Tx's:
      //
      //          txSerial copy:       19s
      //          binaryReader copy:   20s
      //          tx.unserialize:      45s
      //          txMap_[hash] = tx:  115s
      //          &(txMap_[hash]):      0.61s
      //
      //       In other words, we spend a shitload of time copying data around.
      //       If we switch to copying all data once from file, and then only
      //       copy pointers around, we should be in fantastic shape!
      //         
      BinaryStreamBuffer bsb(filename, 25*1024*1024);  // use 25 MB buffer
      
      bool readMagic  = false;
      bool readVarInt = false;
      bool readHeader = false;
      bool readTx     = false;
      uint32_t numBlockBytes;

      BinaryData magicBucket(4);
      BinaryData magicStr(4);
      magicStr.createFromHex(MAGICBYTES);
      BinaryData headHash(32);
      BinaryData headerStr(HEADER_SIZE);

      Tx tempTx;
      BlockHeader tempBH;


      int nBlocksRead = 0;
      // While there is still data left in the stream (file), pull it
      while(bsb.streamPull())
      {
         // Data has been pulled into the buffer, process all of it
         while(bsb.getBufferSizeRemaining() > 1)
         {
            static int i = 0;
            if( !readMagic )
            {
               if(bsb.getBufferSizeRemaining() < 4)
                  break;
               bsb.reader().get_BinaryData(magicBucket, 4);
               if( !(magicBucket == magicStr) )
               {
                  //cerr << "Magic string does not match network!" << endl;
                  //cerr << "\tExpected: " << MAGICBYTES << endl;
                  //cerr << "\tReceived: " << magicBucket.toHex() << endl;
                  break;
               }
               readMagic = true;
            }

            // If we haven't read the blockdata-size yet, do it
            if( !readVarInt )
            {
               // TODO:  Whoops, this isn't a VAR_INT, just a 4-byte num
               if(bsb.getBufferSizeRemaining() < 4)
                  break;
               numBlockBytes = bsb.reader().get_uint32_t();
               readVarInt = true;
            }


            // If we haven't read the header yet, do it
            uint64_t blkByteOffset = bsb.getFileByteLocation();
            if( !readHeader )
            {
               if(bsb.getBufferSizeRemaining() < HEADER_SIZE)
                  break;

               TIMER_WRAP(bsb.reader().get_BinaryData(headerStr, HEADER_SIZE));

               BtcUtils::getHash256(headerStr, headHash);
               TIMER_WRAP(tempBH = BlockHeader(&headerStr, &headHash, blkByteOffset+HEADER_SIZE));
               TIMER_WRAP(headerMap_[headHash] = tempBH);

               //cout << headHash.toHex().c_str() << endl;
               readHeader = true;
            }

            uint32_t txListBytes = numBlockBytes - HEADER_SIZE;
            BlockHeader & blkHead = headerMap_[headHash];
            if( !readTx )
            {
               if(bsb.getBufferSizeRemaining() < txListBytes)
                  break;

               if(justHeaders)
                  bsb.reader().advance((uint32_t)txListBytes);
               else
               {
                  uint8_t varIntSz;
                  TIMER_WRAP(blkHead.numTx_ = (uint32_t)bsb.reader().get_var_int(&varIntSz));
                  blkHead.txPtrList_.resize(blkHead.numTx_);
                  txListBytes -= varIntSz;
                  BinaryData allTx(txListBytes);
                  TIMER_WRAP(bsb.reader().get_BinaryData(allTx.getPtr(), txListBytes));
                  TIMER_WRAP(BinaryReader txListReader(allTx));
                  for(uint32_t i=0; i<blkHead.numTx_; i++)
                  {

                     uint32_t readerStartPos = txListReader.getPosition();
                     TIMER_WRAP(tempTx.unserialize(txListReader));
                     uint32_t readerEndPos = txListReader.getPosition();

                     TIMER_WRAP(BinaryData txSerial( allTx.getPtr() + readerStartPos, 
                                                     allTx.getPtr() + readerEndPos    ));
                     tempTx.nBytes_    = readerEndPos - readerStartPos;
                     tempTx.headerPtr_ = &blkHead;

                     // Calculate the hash of the Tx
                     TIMER_START("TxSerial Hash");
                     BinaryData hashOut(32);
                     BtcUtils::getHash256(txSerial, hashOut);
                     tempTx.thisHash_  = hashOut;
                     TIMER_STOP("TxSerial Hash");

                     //cout << "Tx Hash: " << hashOut.toHex().c_str() << endl;

                     ////////////// Debugging Output - DELETE ME ///////////////
                     //Tx newTx;
                     //BinaryData tempTxSer = tempTx.serialize();
                     //newTx.unserialize(tempTxSer);
                     //BinaryData newTxSer = newTx.serialize();
                     //BtcUtils::getHash256(txSerial, hashOut);
                     //cout << "Tx Hash: " << hashOut.toHex().c_str() << endl;
                     ////////////// Debugging Output - DELETE ME ///////////////

                     // Finally, store it in our map.
                     TIMER_WRAP(txMap_[hashOut] = tempTx);
                     TIMER_WRAP(Tx * txPtr = &(txMap_[hashOut]));
                     

                     if(txPtr == NULL)
                     {
                        cerr << "***Insert Tx Failed! " 
                             << tempTx.thisHash_.toHex().c_str()
                             << endl;
                        // tempTx.print(cout);
                     }

                     TIMER_WRAP(blkHead.txPtrList_[i] = txPtr);

                  }
               }

               readTx = true;
            }

            
            readMagic  = false;
            readVarInt = false;
            readHeader = false;
            readTx     = false;
            nBlocksRead++;

         }
      }

      return (uint32_t)headerMap_.size();
   }
   */



   /////////////////////////////////////////////////////////////////////////////
   /////////////////////////////////////////////////////////////////////////////
   //   
   // Attempting the same thing as above, but using refs only.  This should 
   // actually simplify the buffering because there's only a single, massive
   // copy operation.  And there should be very little extra copying after 
   // that.  Only copying pointers around.
   //
   /////////////////////////////////////////////////////////////////////////////
   /////////////////////////////////////////////////////////////////////////////
   
   /*
   uint32_t importRefsFromBlockFile(std::string filename)
   {
      // TODO: My original everythingRef solution for headers would save a LOT
      //       of computation here.  At 140k blocks, with 1.1 million Tx's:
      //
      //          txSerial copy:       19s
      //          binaryReader copy:   20s
      //          tx.unserialize:      45s
      //          txMap_[hash] = tx:  115s
      //          &(txMap_[hash]):      0.61s
      //
      //       In other words, we spend a shitload of time copying data around.
      //       If we switch to copying all data once from file, and then only
      //       copy pointers around, we should be in fantastic shape!
      //         
      BinaryStreamBuffer bsb(filename, 25*1024*1024);  // use 25 MB buffer
      
      bool readMagic  = false;
      bool readVarInt = false;
      bool readHeader = false;
      bool readTx     = false;
      uint32_t numBlockBytes;

      BinaryData magicBucket(4);
      BinaryData magicStr(4);
      magicStr.createFromHex(MAGICBYTES);
      BinaryData headHash(32);
      BinaryData headerStr(HEADER_SIZE);

      Tx tempTx;
      BlockHeader tempBH;


      int nBlocksRead = 0;
      // While there is still data left in the stream (file), pull it
      while(bsb.streamPull())
      {
         // Data has been pulled into the buffer, process all of it
         while(bsb.getBufferSizeRemaining() > 1)
         {
            static int i = 0;
            if( !readMagic )
            {
               if(bsb.getBufferSizeRemaining() < 4)
                  break;
               bsb.reader().get_BinaryData(magicBucket, 4);
               if( !(magicBucket == magicStr) )
               {
                  //cerr << "Magic string does not match network!" << endl;
                  //cerr << "\tExpected: " << MAGICBYTES << endl;
                  //cerr << "\tReceived: " << magicBucket.toHex() << endl;
                  break;
               }
               readMagic = true;
            }

            // If we haven't read the blockdata-size yet, do it
            if( !readVarInt )
            {
               // TODO:  Whoops, this isn't a VAR_INT, just a 4-byte num
               if(bsb.getBufferSizeRemaining() < 4)
                  break;
               numBlockBytes = bsb.reader().get_uint32_t();
               readVarInt = true;
            }


            // If we haven't read the header yet, do it
            uint64_t blkByteOffset = bsb.getFileByteLocation();
            if( !readHeader )
            {
               if(bsb.getBufferSizeRemaining() < HEADER_SIZE)
                  break;

               TIMER_WRAP(bsb.reader().get_BinaryData(headerStr, HEADER_SIZE));

               BtcUtils::getHash256(headerStr, headHash);
               TIMER_WRAP(tempBH = BlockHeader(&headerStr, &headHash, blkByteOffset+HEADER_SIZE));
               TIMER_WRAP(headerMap_[headHash] = tempBH);

               //cout << headHash.toHex().c_str() << endl;
               readHeader = true;
            }

            uint32_t txListBytes = numBlockBytes - HEADER_SIZE;
            BlockHeader & blkHead = headerMap_[headHash];
            if( !readTx )
            {
               if(bsb.getBufferSizeRemaining() < txListBytes)
                  break;

               if(justHeaders)
                  bsb.reader().advance((uint32_t)txListBytes);
               else
               {
                  uint8_t varIntSz;
                  TIMER_WRAP(blkHead.numTx_ = (uint32_t)bsb.reader().get_var_int(&varIntSz));
                  blkHead.txPtrList_.resize(blkHead.numTx_);
                  txListBytes -= varIntSz;
                  BinaryData allTx(txListBytes);
                  TIMER_WRAP(bsb.reader().get_BinaryData(allTx.getPtr(), txListBytes));
                  TIMER_WRAP(BinaryReader txListReader(allTx));
                  for(uint32_t i=0; i<blkHead.numTx_; i++)
                  {

                     uint32_t readerStartPos = txListReader.getPosition();
                     TIMER_WRAP(tempTx.unserialize(txListReader));
                     uint32_t readerEndPos = txListReader.getPosition();

                     TIMER_WRAP(BinaryData txSerial( allTx.getPtr() + readerStartPos, 
                                                     allTx.getPtr() + readerEndPos    ));
                     tempTx.nBytes_    = readerEndPos - readerStartPos;
                     tempTx.headerPtr_ = &blkHead;

                     // Calculate the hash of the Tx
                     TIMER_START("TxSerial Hash");
                     BinaryData hashOut(32);
                     BtcUtils::getHash256(txSerial, hashOut);
                     tempTx.thisHash_  = hashOut;
                     TIMER_STOP("TxSerial Hash");

                     //cout << "Tx Hash: " << hashOut.toHex().c_str() << endl;

                     ////////////// Debugging Output - DELETE ME ///////////////
                     //Tx newTx;
                     //BinaryData tempTxSer = tempTx.serialize();
                     //newTx.unserialize(tempTxSer);
                     //BinaryData newTxSer = newTx.serialize();
                     //BtcUtils::getHash256(txSerial, hashOut);
                     //cout << "Tx Hash: " << hashOut.toHex().c_str() << endl;
                     ////////////// Debugging Output - DELETE ME ///////////////

                     // Finally, store it in our map.
                     TIMER_WRAP(txMap_[hashOut] = tempTx);
                     TIMER_WRAP(Tx * txPtr = &(txMap_[hashOut]));
                     

                     if(txPtr == NULL)
                     {
                        cerr << "***Insert Tx Failed! " 
                             << tempTx.thisHash_.toHex().c_str()
                             << endl;
                        // tempTx.print(cout);
                     }

                     TIMER_WRAP(blkHead.txPtrList_[i] = txPtr);

                  }
               }

               readTx = true;
            }

            
            readMagic  = false;
            readVarInt = false;
            readHeader = false;
            readTx     = false;
            nBlocksRead++;

         }
      }

      return (uint32_t)headerMap_.size();
   }
   */



   /////////////////////////////////////////////////////////////////////////////
   // Not sure exactly when this would get used...
   void addHeader(BinaryData const & binHeader)
   {
      BinaryData theHash(32); 
      BtcUtils::getHash256(binHeader, theHash);
      headerMap_[theHash] = BlockHeader(&binHeader, &theHash);
   }



   // This returns false if our new main branch does not include the previous
   // topBlock.  If this returns false, that probably means that we have
   // previously considered some blocks to be valid that no longer are valid.
   bool organizeChain(bool forceRebuild=false)
   {
      // If rebuild, we zero out any original organization data and do a 
      // rebuild of the chain from scratch.  This will need to be done in
      // the event that our first call to organizeChain returns false, which
      // means part of blockchain that was previously valid, has become
      // invalid.  Rather than get fancy, just rebuild all which takes less
      // than a second, anyway.
      if(forceRebuild)
      {
         map<BinaryData, BlockHeader>::iterator iter;
         for( iter  = headerMap_.begin(); 
              iter != headerMap_.end(); 
              iter++)
         {
            iter->second.difficultySum_ = -1;
            iter->second.difficultyFlt_ = -1;
            iter->second.blockHeight_   =  0;
            iter->second.isFinishedCalc_ = false;
         }
      }

      // Set genesis block
      BlockHeader & genBlock = getGenesisBlock();
      genBlock.blockHeight_    = 0;
      genBlock.difficultyFlt_  = 1.0;
      genBlock.difficultySum_  = 1.0;
      genBlock.isMainBranch_   = true;
      genBlock.isOrphan_       = false;
      genBlock.isFinishedCalc_ = true;

      BinaryData const & GenesisHash_ = BlockHeader::GenesisHash_;
      BinaryData const & EmptyHash_   = BlockHeader::EmptyHash_;
      if(topBlockPtr_ == NULL)
         topBlockPtr_ = &genBlock;

      // Store the old top block so we can later check whether it is included 
      // in the new chain organization
      BlockHeader* prevTopBlockPtr = topBlockPtr_;

      // Iterate over all blocks, track the maximum difficulty-sum block
      map<BinaryData, BlockHeader>::iterator iter;
      uint32_t maxBlockHeight = 0;
      double   maxDiffSum = 0;
      for( iter = headerMap_.begin(); iter != headerMap_.end(); iter ++)
      {
         // *** The magic happens here
         double thisDiffSum = traceChainDown(iter->second);
         // ***
         
         if(thisDiffSum > maxDiffSum)
         {
            maxDiffSum     = thisDiffSum;
            topBlockPtr_   = &(iter->second);
         }
      }

      // Walk down the list one more time, set nextHash fields
      // Also set headersByHeight_;
      topBlockPtr_->nextHash_ = EmptyHash_;
      BlockHeader* thisBlockPtr = topBlockPtr_;
      bool prevChainStillValid = (thisBlockPtr == prevTopBlockPtr);
      headersByHeight_.resize(topBlockPtr_->getBlockHeight()+1);
      while( !thisBlockPtr->isFinishedCalc_ )
      {
         thisBlockPtr->isFinishedCalc_ = true;
         thisBlockPtr->isMainBranch_   = true;
         headersByHeight_[thisBlockPtr->getBlockHeight()] = thisBlockPtr;

         BinaryData & childHash        = thisBlockPtr->thisHash_;
         thisBlockPtr                  = &(headerMap_[thisBlockPtr->prevHash_]);
         thisBlockPtr->nextHash_       = childHash;

         if(thisBlockPtr == prevTopBlockPtr)
            prevChainStillValid = true;
      }

      // Not sure if this should be automatic... for now I don't think it hurts
      if( !prevChainStillValid )
         organizeChain(true); // force-rebuild the blockchain

      return prevChainStillValid;
   }

private:

   /////////////////////////////////////////////////////////////////////////////
   // Start from a node, trace down to the highest solved block, accumulate
   // difficulties and difficultySum values.  Return the difficultySum of 
   // this block.
   double traceChainDown(BlockHeader & bhpStart)
   {
      if(bhpStart.difficultySum_ > 0)
         return bhpStart.difficultySum_;

      // Prepare some data structures for walking down the chain
      vector<double>          difficultyStack(headerMap_.size());
      vector<BlockHeader*>     bhpPtrStack(headerMap_.size());
      uint32_t blkIdx = 0;
      double thisDiff;

      // Walk down the chain of prevHash_ values, until we find a block
      // that has a definitive difficultySum value (i.e. >0). 
      BlockHeader* thisPtr = &bhpStart;
      map<BinaryData, BlockHeader>::iterator iter;
      while( thisPtr->difficultySum_ < 0)
      {
         thisDiff = BlockHeader::convertDiffBitsToDouble(
                              *(uint32_t*)(thisPtr->diffBitsRaw_.getPtr()));
         difficultyStack[blkIdx] = thisDiff;
         bhpPtrStack[blkIdx]     = thisPtr;
         blkIdx++;


         iter = headerMap_.find(thisPtr->prevHash_);
         if( iter != headerMap_.end() )
            thisPtr = &(iter->second);
         else
         {
            // We didn't hit a known block, but we don't have this block's
            // ancestor in the memory pool, so this is an orphan chain...
            // at least temporarily
            markOrphanChain(bhpStart);
            return 0.0;
         }
      }


      // Now we have a stack of difficulties and pointers.  Walk back up
      // (by pointer) and accumulate the difficulty values 
      double   seedDiffSum = thisPtr->difficultySum_;
      uint32_t blkHeight   = thisPtr->blockHeight_;
      for(int32_t i=blkIdx-1; i>=0; i--)
      {
         seedDiffSum += difficultyStack[i];
         blkHeight++;
         thisPtr                 = bhpPtrStack[i];
         thisPtr->difficultyFlt_ = difficultyStack[i];
         thisPtr->difficultySum_ = seedDiffSum;
         thisPtr->blockHeight_   = blkHeight;
      }
      
      // Finally, we have all the difficulty sums calculated, return this one
      return bhpStart.difficultySum_;
     
   }


   /////////////////////////////////////////////////////////////////////////////
   void markOrphanChain(BlockHeader & bhpStart)
   {
      bhpStart.isOrphan_ = true;
      bhpStart.isMainBranch_ = false;
      map<BinaryData, BlockHeader>::iterator iter;
      iter = headerMap_.find(bhpStart.getPrevHash());
      while( iter != headerMap_.end() )
      {
         iter->second.isOrphan_ = true;
         iter->second.isMainBranch_ = false;
         iter = headerMap_.find(iter->second.prevHash_);
      }
   }


   
};



#endif