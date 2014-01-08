from armoryengine.ArmoryUtils import *
from armoryengine.BinaryUnpacker import *
from armoryengine.BinaryPacker import *
from armoryengine.PyBtcAddress import *
from armoryengine.PyBtcWallet import *
from qtdialogs import *

class InvalidEntry(Exception): pass

class PyBtcWalletRecovery(object):
   """
   Fail safe wallet recovery tool. Reads a wallet, verifies and extracts sensitive data to a new file.

   """

   #############################################################################
   def BuildLogFile(self, errorCode=0):
      """
      The recovery function has ended and called this. Review the analyzed data, build a log and return negative values if the recovery couldn't complete
      
      """
      if self.GUI:
         self.UIreport = self.UIreport + '<b>- Building log file...</b><br>'
         self.ProgDlg.UpdateText(self.UIreport)
         
      if self.newwalletPath != None:   
         self.LogPath = self.newwalletPath + ".log"
      else: 
         self.LogPath = self.WalletPath + ".log"
      basename = os.path.basename(self.LogPath)
            
      self.logfile = open(self.LogPath, 'ab')
      self.logfile.write('Recovering wallet %s on %s \r\n' % (basename, time.ctime()))
      self.logfile.write('Using %s recovery mode\r\n' % (self.smode))
      
      if errorCode != 0:
         if errorCode == -1:
            errorstr = 'ERROR: Invalid path, or file is not a valid Armory wallet\r\n'
         elif errorCode == -2:
            errorstr = 'ERROR: file I/O failure. Do you have proper credentials?\r\n'
         elif errorCode == -3:
            errorstr = 'ERROR: This wallet file is for another network/blockchain\r\n'
         elif errorCode == -4:
            errorstr = 'ERROR: invalid or missing passphrase for encrypted wallet\r\n'
         elif errorCode == -10:
            errorstr = 'ERROR: no kdf parameters available\r\n'
         elif errorCode == -12:
            errorstr = 'ERROR: failed to unlock root key\r\n'
         
         self.logfile.write('   %s' % (errorstr))
         if self.GUI:
            self.UIreport = self.UIreport + errorstr
            self.ProgDlg.UpdateText(self.UIreport)            
         return self.EndLog(errorCode)      
           
      if self.WO == 1:
         self.logfile.write('Wallet is Watch Only\r\n')
      else:
         self.logfile.write('Wallet contains private keys ')
         if self.useEnc == 0:
            self.logfile.write('and doesn\'t use encryption\r\n')
         else:
            self.logfile.write('and uses encryption')
            
      if self.smode == 'stripped' and self.WO == 0:
         self.logfile.write('   Recovered root key and chaincode, stripped recovery done.')
         return self.EndLog(errorCode)
            
      self.logfile.write('The wallet file is %d  bytes, of which %d bytes were readable\r\n' % (self.fileSize, self.dataLastOffset))
      self.logfile.write('%d chain addresses, %d imported keys and %d comments were found\r\n' % (self.naddress, self.nImports, self.ncomments))
            
      nErrors = 0
      #### chained keys      
      self.logfile.write('Found %d chained address entries\r\n' % (self.naddress))
      
      if len(self.byteError) == 0:
         self.logfile.write('No byte errors were found in the wallet file\r\n')
      else:
         nErrors = nErrors + len(self.byteError)
         self.logfile.write('%d byte errors were found in the wallet file:\r\n' % (len(self.byteError)))
         for i in range(0, len(self.byteError)):
            self.logfile.write('   chainIndex %s at file offset %s\r\n' % (self.byteError[i][0], self.byteError[i][1]))
 
      
      if len(self.brokenSequence) == 0:
         self.logfile.write('All chained addresses were arranged sequentially in the wallet file\r\n')
      else:
         nErrors = nErrors + len(self.brokenSequence)
         self.logfile.write('The following %d addresses were not arranged sequentially in the wallet file:\r\n' % (len(self.brokenSequence)))
         for i in range(0, len(self.brokenSequence)):
            self.logfile.write('   chainIndex %s at file offset %s\r\n' % (self.brokenSequence[i][0], self.brokenSequence[i][1]))
            
      if len(self.sequenceGaps) == 0:
         self.logfile.write('There are no gaps in the address chain\r\n')
      else:
         nErrors = nErrors + len(self.sequenceGaps)
         self.logfile.write('Found %d gaps in the address chain:\r\n' % (len(self.sequenceGaps)))
         for i in range(0, len(self.sequenceGaps)):
            self.logfile.write('   from chainIndex %d to %d\r\n' % (self.sequenceGaps[i][0], self.sequenceGaps[i][1]))
      
      if len(self.brokenPublicKeyChain) == 0:      
         self.logfile.write('No invalid chained public address was found\r\n')
      else:
         nErrors = nErrors + len(self.brokenPublicKeyChain)
         self.logfile.write('Found %d invalid chained public addresses:\r\n' % (len(self.brokenPublicKeyChain)))
         for i in range(0, len(self.brokenPublicKeyChain)):
            self.logfile.write('   at chainIndex %d, file offset %s\r\n' % (self.brokenPublicKeyChain[i][0], self.brokenPublicKeyChain[i][1]))
            
      if len(self.chainCodeCorruption) == 0:
         self.logfile.write('No chaincode corruption was found\r\n')
      else:
         nErrors = nErrors + len(self.chainCodeCorruption)
         self.logfile.write('Found %d instances of chaincode corruption:\r\n' % (len(self.chainCodeCorruption)))
         for i in range(0, len(self.chainCodeCorruption)):
            self.logfile.write('   at chainIndex %d, file offset %d\r\n' % (self.chainCodeCorruption[i][0], self.chainCodeCorruption[i][1]))
            
      if len(self.invalidPubKey) == 0:
         self.logfile.write('All chained public keys are valid EC points\r\n')
      else:
         nErrors = nErrors + len(self.invalidPubKey)
         self.logfile.write('%d chained public keys are invalid EC points:\r\n' % (len(self.invalidPubKey)))
         for i in range(0, len(self.invalidPubKey)):
            self.logfile.write('   at chainIndex %d, file offset %d' % (self.invalidPubKey[i][0], self.invalidPubKey[i][1]))
            
      if len(self.missingPubKey) == 0:
         self.logfile.write('No chained public key is missing\r\n')
      else:
         nErrors = nErrors + len(self.missingPubKey)
         self.logfile.write('%d chained public keys are missing:\r\n' % (len(self.missingPubKey)))
         for i in range(0, len(self.missingPubKey)):
            self.logfile.write('   at chainIndex %d, file offset %d' % (self.missingPubKey[i][0], self.missingPubKey[i][1]))

      if len(self.hashValMismatch) == 0:
         self.logfile.write('All entries were saved under their matching hashVal\r\n')
      else:
         nErrors = nErrors + len(self.hashValMismatch)
         self.logfile.write('%d address entries were saved under an erroneous hashVal:\r\n' % (len(self.hashValMismatch)))
         for i in range(0, len(self.hashValMismatch)):
            self.logfile.write('   at chainIndex %d, file offset %d\r\n' % (self.hashValMismatch[i][0], self.hashValMismatch[i][1]))
      
      if self.WO == 0:  
         if len(self.unmatchedPair) == 0:
            self.logfile.write('All chained public keys match their respective private keys\r\n')
         else:
            nErrors = nErrors + len(self.unmatchedPair)
            self.logfile.write('%d public keys do not match their respective private key:\r\n' % (len(self.unmatchedPair)))
            for i in range(0, len(self.unmatchedPair)):
               self.logfile.write('   at chainIndex %d, file offset %d\r\n' % (self.unmatchedPair[i][0], self.unmatchedPair[i][1]))
            
      if len(self.misc) > 0:
         nErrors = nErrors + len(self.misc)
         self.logfile.write('%s miscalleneous errors were found:\r\n')
         for i in range(0, len(self.misc)):
            self.logfile.write('   %d' % self.misc[i])
      
      #### imported keys      
      self.logfile.write('Found %d imported address entries\r\n' % (self.nImports))
      
      if self.nImports > 0:
         if len(self.importedErr) == 0:
            self.logfile.write('No errors were found within the imported address entries\r\n')
         else:
            nErrors = nErrors + len(self.importedErr)
            self.logfile.write('%d errors were found within the imported address entries:\r\n' % (len(self.importedErr)))
            for i in range(0, len(self.importedErr)):
               self.logfile.write('   %s' % (self.importedErr[i]))
      
      ####TODO: comments error log
            
      self.UIreport = self.UIreport + '<b>- %d errors where found</b><br>' % (nErrors)
      self.EndLog()
      return 0 
   
   #############################################################################
   def EndLog(self, errorcode=0):
      if errorcode < 0:
         self.logfile.write('Recovery failed: error code %d\r\n\r\n\r\n' % (errorcode))
         
         if self.GUI:
            self.UIreport = self.UIreport + '<b>- Recovery failed: error code %d</b><br>' % (errorcode)
            self.ProgDlg.UpdateText(self.UIreport)
      else:
         self.logfile.write('Recovery done\r\n\r\n\r\n')
         
         if self.GUI:
            self.UIreport = self.UIreport + '<b>- Recovery done</b><br>'
            self.UIreport = self.UIreport + '<br>Recovered wallet saved at:<br>- %s<br>' % (self.newwalletPath)
            self.ProgDlg.UpdateText(self.UIreport)
         
      if self.GUI:
         self.UIreport = self.UIreport + '<br>Recovery log saved at:<br>- %s<br>' % (self.LogPath)
         self.ProgDlg.UpdateText(self.UIreport, True)
      self.logfile.close()
      
      return errorcode
   
   #############################################################################
   def RecoverWallet(self, WalletPath, Passphrase=None, Mode='Bare', GUI=False):
      if GUI == True:
         self.ProgressRdy = 0
         rtv = self.ProcessWallet(WalletPath, Passphrase, Mode, GUI, mainWnd=self.parent, async = True)
         self.UIProgress()
         return rtv
      else:
         return self.ProcessWallet(WalletPath, Passphrase, Mode, GUI)
      
   #############################################################################
   @AllowAsync
   def ProcessWallet(self, WalletPath, Passphrase=None, Mode='Bare', GUI=False, mainWnd=None):
      """
      Modes:
         1) Stripped: Only recover the root key and chaincode (it all sits in the header). As fail safe as it gets.

         2) Bare: Recover root key, chaincode and valid private/public key pairs. Verify integrity of the wallet and consistency of all entries encountered.
            Skips comments, unprocessed public keys and otherwise corrupted data without attempting to fix it.

         3) Full: Recovers as much data as possible from the wallet.
         
         4) Meta: Get all labels and comment entries from the wallet, return as list
         
         5) Check: checks wallet for consistency. Does not yield a recovered file, does not enforce unlocking encrypted wallets.
         
         returned values:
         -1: invalid path or file isn't a wallet
         
         in meta mode, a dict is returned holding all comments and labels in the wallet
      """
      
      RecoveredWallet = None
      self.WalletPath = WalletPath
      self.newwalletPath = None
      self.WO = 0
      self.GUI = GUI
      self.UIreport = ''
            
      self.naddress = 0
      addrDict = {} #holds address chain sequentially, ordered by chainIndex, as lists: [addrEntry, hashVal, naddress, byteLocation, rawData]
      
      self.nImports = 0
      importedDict = {} #holds imported address, by order of apparition, as lists: [addrEntry, hashVal, byteLocation, rawData]
      
      self.ncomments = 0
      commentDict = {} #holds all comments entries, as lists: [rawData, hashVal, dtype]
      #in meta mode, the wallet's short and long labels are saved in entries shortLabel and longLabel, pointing to a single str object
      
      #wait for the progress window to be created
      if GUI == True:
         while self.ProgressRdy == 0: 
            time.sleep(0.01)
         self.UIreport = '<b>Recovering wallet:</b> %s<br>' % (os.path.basename(WalletPath))
         self.ProgDlg.UpdateText(self.UIreport)      

      rmode = 2
      self.smode = 'bare'
      if Mode == 'Stripped' or Mode == 1: 
         rmode = 1
         self.smode = 'stripped'
      elif Mode == 'Full' or Mode == 3: 
         rmode = 3
         self.smode = 'full'
      elif Mode == 'Meta' or Mode == 4: 
         rmode = 4
         self.smode = 'meta'
         self.WO = 1
      elif Mode == 'Check' or Mode == 5:
         rmode = 5
         self.smode = 'consistency check'

      self.fileSize=0
      if not os.path.exists(WalletPath): return self.BuildLogFile(-1)
      else: self.fileSize = os.path.getsize(WalletPath)

      toRecover = PyBtcWallet()
      toRecover.walletPath = WalletPath
      toRecover.mainWnd = mainWnd

      #consistency check
      try:
         toRecover.doWalletFileConsistencyCheck()
      except: #I expect 99% of errors raised here would be by the os package failing on I/O operations, mainly for lack of credentials. 
         return self.BuildLogFile(-2)

      #fetch wallet content
      wltfile = open(WalletPath, 'rb')
      wltdata = BinaryUnpacker(wltfile.read())
      wltfile.close()

      #unpack header
      try:
         returned = toRecover.unpackHeader(wltdata)
      except: return self.BuildLogFile(-1) #Raises here come from invalid header parsing, meaning the file isn't an Armory wallet to begin with, or the header is fubar

      if returned < 0: return self.BuildLogFile(WalletPath, -3)
         
      self.useEnc=0

      #check for private keys (watch only?)
      if toRecover.watchingOnly is True:
         self.WO = 1
         
      if self.WO == 0:
         #check if wallet is encrypted
         if toRecover.isLocked==True and Passphrase==None and rmode != 4:
            #locked wallet and no passphrase, prompt the user if we're using the gui           
            if GUI==True:
               self.ProgDlg.AskUnlock(toRecover)
               while self.ProgDlg.GotPassphrase == 0:
                  time.sleep(0.01)
                  
               if self.ProgDlg.GotPassphrase == -1:
                  if rmode==5: self.WO = 1
                  else: return self.BuildLogFile(-4)
               
               Passphrase = self.ProgDlg.Passphrase
               self.ProgDlg.Passphrase = None   
               
            else:
               if rmode==5: self.WO = 1
               else: return self.BuildLogFile(-4)
            
         newAddr = toRecover.addrMap['ROOT']

         #if the wallet uses encryption, unlock ROOT and verify it
         SecurePassphrase = None
         if toRecover.isLocked and self.WO==0:
            self.useEnc
            SecurePassphrase = SecureBinaryData(Passphrase)
            Passphrase = None
            if not toRecover.kdf: return self.BuildLogFile(-10)
            
            secureKdfOutput = toRecover.kdf.DeriveKey(SecurePassphrase)

            if not toRecover.verifyEncryptionKey(secureKdfOutput): 
               SecurePassphrase.destroy()
               secureKdfOutput.destroy()
               return self.BuildLogFile(-4)
            
            #DlgUnlockWallet may have filled kdfKey. Since this code can be called with no UI and just the passphrase, gotta make sure this member is cleaned up before setting it
            if isinstance(toRecover.kdfKey, SecureBinaryData): toRecover.kdfKey.destroy()
            toRecover.kdfKey = secureKdfOutput

            try:
               newAddr.unlock(toRecover.kdfKey)
            except:
               return self.BuildLogFile(-12)
         else:
            SecurePassphrase = None
            
         #create recovered wallet
         if rmode < 4:
            RecoveredWallet = PyBtcWallet()
            self.newwalletPath = os.path.join(os.path.dirname(toRecover.walletPath), 'armory_%s_RECOVERED.wallet' % (toRecover.uniqueIDB58))
            if os.path.exists(self.newwalletPath):
               try: os.remove(self.newwalletPath)
               except: return self.BuildLogFile(-2)

            try:
               RecoveredWallet.createNewWallet(newWalletFilePath=self.newwalletPath, securePassphrase=SecurePassphrase, \
                                            plainRootKey=newAddr.binPrivKey32_Plain, chaincode=newAddr.chaincode, \
                                            #not registering with the BDM, so no addresses are computed
                                            doRegisterWithBDM=False, \
                                            shortLabel=toRecover.labelName, longLabel=toRecover.labelDescr)
            except:
               return self.BuildLogFile(-2) #failed to create new file
            
            if SecurePassphrase is not None:
               RecoveredWallet.kdfKey = RecoveredWallet.kdf.DeriveKey(SecurePassphrase)
         
            #all wallets have their kdfKey at this point, dont need the passphrase anymore
            if SecurePassphrase is not None:
               SecurePassphrase.destroy()
               SecurePassphrase = None

            if rmode == 1:
               if isinstance(toRecover.kdfKey, SecureBinaryData): toRecover.kdfKey.destroy()
               if isinstance(RecoveredWallet.kdfKey, SecureBinaryData): RecoveredWallet.kdfKey.destroy() 
               return self.BuildLogFile() #stripped recovery, we are done

      if rmode == 4:
         commentDict["shortLabel"] = toRecover.labelName
         commentDict["longLabel"]  = toRecover.labelDescr      
      
      
      #address entries may not be saved sequentially. To check the address chain is valid, all addresses will be unserialized
      #and saved by chainIndex in addrDict. Then all addresses will be checked for consistency and proper chaining. Imported 
      #private keys and comments will be added at the tail of the file.
      
      UIupdate = ""
      self.misc = [] #miscellaneous errors     
      self.rawError = [] #raw binary errors 
      
      #move on to wallet body
      toRecover.lastComputedChainIndex = -UINT32_MAX
      toRecover.lastComputedChainAddr160  = None
      while wltdata.getRemainingSize()>0:
         byteLocation = wltdata.getPosition()
         
         if GUI:
            UIupdate =  '<b>- Parsing file:</b>   %d/%d kB<br>' % (byteLocation, self.fileSize)
            if self.ProgDlg.UpdateText(self.UIreport + UIupdate) == 0:
               return 0
            
         newAddr = None
         try:
            dtype, hashVal, rawData = toRecover.unpackNextEntry(wltdata)
         except NotImplementedError:
            self.misc.append('Found OPEVAL data entry at offest: %d' % (byteLocation))
            pass
         except:
            #Error in the binary file content. Try to skip an entry size amount of bytes to find a valid entry.
            self.rawError.append('Raw binary error found at offset: %d' % (byteLocation))            
            
            dtype, hashVal, rawData, dataList = self.LookForFurtherEntry(wltdata, byteLocation)
            
            if dtype is None:
               #could not find anymore valid data
               self.rawError.append("Could not find anymore valid data past offset: %d" % (byteLocation))
               break
            
            byteLocation = dataList[1]
            self.rawError.append('   Found a valid data entry at offset: %d' % (byteLocation))

            if dataList[0] == 0:
               #found an address entry, but it has checksum errors
               newAddr = dataList[2]
               
         if dtype==WLT_DATATYPE_KEYDATA:
            if rmode != 4:
               
               if newAddr is None:
                  newAddr = PyBtcAddress()
                  try:
                     newAddr.unserialize(rawData)
                  except:
                     #unserialize error, try to recover the entry
                     self.rawError.append('   Found checksum errors in address entry starting at offset: %d' % (byteLocation))
                     try:                        
                        newAddr, chksumError = self.addrEntry_unserialize_recover(rawData)
                        self.rawError.append('   Recovered damaged entry')
                     except:
                        #failed to recover the entry
                        self.rawError.append('   Could not recover damaged entry')
                        newAddr = None
               
               if newAddr is not None:
                  newAddr.walletByteLoc = byteLocation + 21
      
                  if newAddr.useEncryption:
                     newAddr.isLocked = True
                   
                  #save address entry count in the file, to check for entry sequence
                  if newAddr.chainIndex > -2 :
                     addrDict[newAddr.chainIndex] = [newAddr, hashVal, self.naddress, byteLocation, rawData]
                     self.naddress = self.naddress +1
                  else:
                     importedDict[self.nImports] = [newAddr, hashVal, byteLocation, rawData]
                     self.nImports = self.nImports +1
                     
            else: self.naddress = self.naddress +1
               

         elif dtype in (WLT_DATATYPE_ADDRCOMMENT, WLT_DATATYPE_TXCOMMENT):
            if rmode > 2: 
               commentDict[self.ncomments] = [rawData, hashVal, dtype]
               self.ncomments = self.ncomments +1

         elif dtype==WLT_DATATYPE_OPEVAL:
            self.misc.append('Found OPEVAL data entry at offest: %d' % (byteLocation))
            pass
         elif dtype==WLT_DATATYPE_DELETED:
            pass
         else:
            self.misc.append('Found unknown data entry type at offset: %d' % (byteLocation))
            #TODO: try same trick as to recover from unpack errors
            
      self.dataLastOffset = wltdata.getPosition()
      UIupdate = '<b>- Parsing file:</b>   %d/%d kB<br>' % (self.dataLastOffset, self.fileSize)
      self.UIreport = self.UIreport + UIupdate

      #TODO: verify chainIndex 0 was derived from the root key

      if rmode != 4: 
         currSequence = addrDict[0][2]
         chaincode = addrDict[0][0].chaincode.toHexStr()
      else: 
         currSequence = None
         chaincode = None
         commentDict['naddress'] = self.naddress
         self.naddress = 0
         commentDict['ncomments'] = self.ncomments
         
      """
      Set of lists holding various errors at given indexes. Used at the end of the recovery process to compile a wallet specific log of encountered
      inconsistencies
      """
      self.byteError = [] #byte errors
      self.brokenSequence = [] #inconsistent address entry order in the file
      self.sequenceGaps = [] #gaps in key pair chain
      self.brokenPublicKeyChain = [] #for public keys: (N-1)*chaincode != N
      self.chainCodeCorruption = [] #addr[N] chaincode doesnt match addr[0] chaincode
      self.invalidPubKey = [] #pub key isnt a valid EC point
      self.missingPubKey = [] #addr[N] has no pub key
      self.hashValMismatch = [] #addrStr20 doesnt match hashVal entry in file      
      self.unmatchedPair = [] #private key doesnt yield public key
      self.importedErr = [] #all imported keys related errors
      
      
      #chained key pairs. for rmode is 4, no need to skip this part, naddress will be 0
      for i in range(0, self.naddress):      
         entrylist = []
         entrylist = list(addrDict[i])
         newAddr = entrylist[0]
         rawData = entrylist[4]
         byteLocation = entrylist[3]
         
         if GUI:
            UIupdate = '<b>- Processing address entries:</b>   %d/%d<br>' % (newAddr.chainIndex +1, self.naddress)
            if self.ProgDlg.UpdateText(self.UIreport + UIupdate) == 0:
               return 0
         
         # Fix byte errors in the address data         
         fixedAddrData = newAddr.serialize()
         if not rawData==fixedAddrData:
            self.byteError([newAddr.chainIndex, byteLocation])
            newAddr = PyBtcAddress()
            newAddr.unserialize(fixedAddrData)
            entrylist[0] = newAddr
            addrDict[i] = entrylist
        
         #check public key is a valid EC point
         if newAddr.hasPubKey():
            if not CryptoECDSA().VerifyPublicKeyValid(newAddr.binPublicKey65):
               self.invalidPubKey.append([newAddr.chainIndex, byteLocation])
         else: self.missingPubKey.append([newAddr.chainIndex, byteLocation])
         
         #check chaincode consistency
         newCC = newAddr.chaincode.toHexStr()
         if newCC != chaincode:
            self.chainCodeCorruption.append([newAddr.chainIndex, byteLocation])
            
         #check the address entry sequence
         nextSequence = entrylist[2]
         if nextSequence != currSequence:
            if (nextSequence - currSequence) != 1:
               self.brokenSequence.append([newAddr.chainIndex, byteLocation])
         currSequence = nextSequence
         
         #check for gaps in the sequence
         if newAddr.chainIndex > 0:
            seq = newAddr.chainIndex -1
            prevEntry = []
            while seq > 0:
               try:
                  prevEntry = addrDict[seq]
                  break
               except:
                  continue
               seq = seq -1
               
            prevEntry = list(addrDict[seq])
            
            gap = newAddr.chainIndex - seq
            if gap > 1:
               self.sequenceGaps.append([seq, newAddr.chainIndex])
         
            #check public address chain
            if newAddr.hasPubKey():
               prevAddr = prevEntry[0]
               
               cid = 0
               extended = prevAddr.binPublicKey65
               while cid < gap:
                  extended = CryptoECDSA().ComputeChainedPublicKey(extended, prevAddr.chaincode)
                  cid = cid +1
                  
               if extended.toHexStr() != newAddr.binPublicKey65.toHexStr():
                  self.brokenPublicKeyChain.append([newAddr.chainIndex, byteLocation])
            

         if self.WO == 0:            
            #not a watch only wallet, check private key chaining and integrity   
           
            #check private key against public key, possibly compute missing ones, depending on the recovery mode
            
            #TODO: if the public key is forked but it's private key matches, save as imported key pair
            #      if a private key forked, save as imported pair
            
            keymismatch=0
            """
            0: public key matches private key
            1: public key doesn't match private key
            2: private key is missing (encrypted)
            3: public key is missing
            4: private key is missing (unencrypted)
            """
            if not newAddr.hasPrivKey():
               #entry has no private key
               keymismatch=2

            if newAddr.chainIndex > 0:
               #chained private key
               if newAddr.createPrivKeyNextUnlock:
                     keymismatch=2
                     
                     #have to build the private key on unlock; we can use prevAddr for that purpose, used to chain the public key off of
                     if newAddr.createPrivKeyNextUnlock:
                        newAddr.createPrivKeyNextUnlock_IVandKey[0] = prevAddr.binInitVect16.copy()
                        newAddr.createPrivKeyNextUnlock_IVandKey[1] = prevAddr.binPrivKey32_Encr.copy()

                        newAddr.createPrivKeyNextUnlock_ChainDepth = newAddr.chainIndex - prevAddr.chainIndex
               else:
                  if not newAddr.useEncryption:
                     #uncomputed private key in a non encrypted wallet? definitely not supposed to happen
                     keymismatch = 4
                     self.misc.append('uncomputed private key in unencrypted wallet at chainIndex %d in wallet %s' % (newAddr.chainIndex, WalletPath))
                  
            #unlock if necessary      
            if keymismatch == 0 or keymismatch == 2:               
               if newAddr.isLocked:     
                  try:
                     newAddr.unlock(toRecover.kdfKey)
                     keymismatch = 0
                  except KeyDataError: 
                     keymismatch=1

            #deal with mismatch scenarios   
            if keymismatch == 1:
               newAddr.binPublicKey65 = CryptoECDSA().ComputePublicKey(self.binPrivKey32_Plain)
               self.unmatchedPair.append(newAddr.chainIndex)
               keymismatch = 0
            
            #TODO: needs better handling for keymismatch == 2   
            elif keymismatch == 2:
               LOGERROR('no private at chainIndex %d in wallet %s' % (newAddr.chainIndex, WalletPath))
               
            elif keymismatch == 3:
               newAddr.binPublicKey65 = CryptoECDSA().ComputePublicKey(self.binPrivKey32_Plain)
               newAddr.addrStr20 = newAddr.binPublicKey65.getHash160()                  
               keymismatch = 0

            #if we have clear possible mismatches (or there were none), proceed to consistency checks
            if keymismatch == 0:
               if not CryptoECDSA().CheckPubPrivKeyMatch(newAddr.binPrivKey32_Plain, newAddr.binPublicKey65):
                  self.unmatchedPair.append([newAddr.chainIndex, byteLocation])
                 
               if newAddr.addrStr20 != entrylist[1]:
                  self.hashValMismatch.append([newAddr.chainIndex, byteLocation])
   
            if newAddr.useEncryption:
               newAddr.lock(toRecover.kdfKey)
   
      if GUI and self.naddress > 0: self.UIreport = self.UIreport + UIupdate

      #imported addresses
      for i in range(0, self.nImports):
         entrylist = []
         entrylist = list(importedDict[i])
         newAddr = entrylist[0]
         rawData = entrylist[3]

         if GUI:
            UIupdate = '<b>- Processing imported address entries:</b>   %d/%d<br>' % (i +1, self.nImports)
            if self.ProgDlg.UpdateText(self.UIreport + UIupdate) == 0:
               return 0
            
         # Fix byte errors in the address data         
         fixedAddrData = newAddr.serialize()
         if not rawData==fixedAddrData:
            self.importedErr.append('found byte error in imported address %d at file offset %d' % (i, entrylist[2]))
            newAddr = PyBtcAddress()
            newAddr.unserialize(fixedAddrData)
            entrylist[0] = newAddr
            importedDict[i] = entrylist
        
         #check public key is a valid EC point
         if newAddr.hasPubKey():
            if not CryptoECDSA().VerifyPublicKeyValid(newAddr.binPublicKey65):
               self.importedErr.append('invalid pub key for imported address %d at file offset %d\r\n' % (i, entrylist[2]))
         else:
            self.importedErr.append('missing pub key for imported address %d at file offset %d\r\n' % (i, entrylist[2]))
         
         #if there a private key in the entry, check for consistency   
         if not newAddr.hasPrivKey():
            self.importedErr.append('missing private key for imported address %d at file offset %d\r\n' % (i, entrylist[2]))
         else:
            keymismatch = 0
            if newAddr.isLocked:
               try:
                  newAddr.unlock(toRecover.kdfKey)
               except KeyDataError:
                  keymismatch = 1
                  self.importedErr.append('pub key doesnt match private key for imported address %d at file offset %d\r\n' % (i, entrylist[2]))
            
                  
            if keymismatch == 0:
               #pubkey is present, check against priv key
               if not CryptoECDSA().CheckPubPrivKeyMatch(newAddr.binPrivKey32_Plain, newAddr.binPublicKey65):
                  keymismatch = 1
                  self.importedErr.append('pub key doesnt match private key for imported address %d at file offset %d\r\n' % (i, entrylist[2]))
            
            if keymismatch == 1:
               #compute missing/invalid pubkey                    
               newAddr.binPublicKey65 = CryptoECDSA().ComputePublicKey(newAddr.binPrivKey32_Plain)
 
            #check hashVal   
            if newAddr.addrStr20 != entrylist[1]:
               newAddr.addrStr20 = newAddr.binPublicKey65.getHash160()
               self.importedErr.append('hashVal doesnt match addrStr20 for imported address %d at file offset %d\r\n' % (i, entrylist[2]))
               
            #if the entry was encrypted, lock it back with the new wallet kdfkey
            if newAddr.useEncryption:
               newAddr.keyChanged = 1
               newAddr.lock(RecoveredWallet.kdfKey)
      
      if GUI and self.nImports > 0: self.UIreport = self.UIreport + UIupdate     
      #TODO: check comments consistency
      
      if self.WO==0:
         if rmode < 4:
            #build address pool
            for i in range(1, self.naddress):
               if GUI:
                  UIupdate = '<b>- Building address chain:</b>   %d/%d<br>' % (i+1, self.naddress)
                  if self.ProgDlg.UpdateText(self.UIreport + UIupdate) == 0:
                     return 0
               
               #TODO: check this builds the proper address chain, and saves encrypted private keys
               RecoveredWallet.computeNextAddress(None, False, False)
               
            if GUI and self.naddress > 0: self.UIreport = self.UIreport + UIupdate
         
            #save imported addresses
            for i in range(0, self.nImports):
               
               if GUI:
                  UIupdate = '<b>- Saving imported addresses:</b>   %d/%d<br>' % (i+1, self.nImports)
                  if self.ProgDlg.UpdateText(self.UIreport + UIupdate) == 0:
                     return 0
                  
               entrylist = []
               entrylist = list(importedDict[i])
               newAddr = entrylist[0]            
               RecoveredWallet.walletFileSafeUpdate([[WLT_UPDATE_ADD, WLT_DATATYPE_KEYDATA, newAddr.addrStr20, newAddr]])
               
            if GUI and self.nImports > 0: self.UIreport = self.UIreport + UIupdate
                     
            #save comments
            if rmode == 3:
               for i in range(0, self.ncomments):
                  if GUI:
                     UIupdate = '<b>- Saving comment entries:</b>   %d/%d<br>' % (i+1, self.ncomments)
                     if self.ProgDlg.UpdateText(self.UIreport + UIupdate) == 0:
                        return 0
                  
                  entrylist = []
                  entrylist = list(commentDict[i])    
                  RecoveredWallet.walletFileSafeUpdate([[WLT_UPDATE_ADD, entrylist[2], entrylist[1], entrylist[0]]]) 
               
               if GUI and self.ncomments > 0: self.UIreport = self.UIreport + UIupdate
      
      
      #TODO: nothing to process anymore at this point. if the recovery mode is 4 (meta), just return the comments dict
      if isinstance(toRecover.kdfKey, SecureBinaryData): toRecover.kdfKey.destroy()   
      if RecoveredWallet is not None: 
         if isinstance(RecoveredWallet.kdfKey, SecureBinaryData): RecoveredWallet.kdfKey.destroy()

      if rmode != 4: return self.BuildLogFile(0)
      else:
          
         return commentDict

   #############################################################################
   def LookForFurtherEntry(self, rawdata, loc):
      """
      Attempts to find valid data entries in wallet file by skipping known byte widths. 
      
      The process:
      1) Assume an address entry with invalid data type key and/or the hash160. Read ahead and try to unserialize a valid PyBtcAddress
      2) Assume a corrupt address entry. Move 1+20+237 bytes ahead, try to unpack the next entry
      
      At this point all entries are of random length. The most accurate way to define them as valid is to try and unpack the next entry,
      or check end of file has been hit gracefully
      
      3) Try for address comment
      4) Try for transaction comment
      5) Try for deleted entry
      
      6) At this point, can still try for random byte search. Essentially, push an incremental amount of bytes until a valid entry or the end of the file
      is hit. Simplest way around it is to recursively call this member with an incremented loc
      
       
      
      About address entries: currently, the code tries to fully unserialize tentative address entries.
      It will most likely raise at the slightest error. However, that doesn't mean the entry is entirely bogus,
      or not an address entry at all. Individual data packets should be checked against their checksum for 
      validity in a full implementation of the raw data recovery layer of this tool. Other entries do not carry
      checksums and thus appear opaque to this recovery layer.
      
      TODO: 
         1) verify each checksum data block in address entries
         2) same with the file header
      """
      
      #check loc against data end.
      if loc >= rawdata.getSize():
         return None, None, None, [0]
      
      #reset to last known good offset
      rawdata.resetPosition(loc)
      
      #try for address entry: push 1 byte for the key, 20 for the public key hash, try to unpack the next 237 bytes as an address entry
      try:
         rawdata.advance(1)         
         hash160 = rawdata.get(BINARY_CHUNK, 20)
         chunk = rawdata.get(BINARY_CHUNK, self.pybtcaddrSize)         
      
         newAddr, chksumError = self.addrEntry_unserialize_recover(chunk)
         #if we got this far, no exception was raised, return the valid entry and hash, but invalid key
                  
         if chksumError != 0:
            #had some checksum errors, pass the data on
            return 0, hash160, chunk, [0, loc, newAddr, chksumError]
         
         return 0, hash160, chunk, [1, loc]
      except:
         #unserialize error, move on
         rawdata.resetPosition(loc)
         
      #try for next entry
      try:
         rawdata.advance(1+20+237)         
         dtype, hash, chunk = PyBtcWallet().unpackNextEntry(rawdata)
         if dtype>-1 and dtype<5:
            return dtype, hash, chunk, [1, loc +1+20+237]
         else:
            rawdata.resetPosition(loc)
      except:
         rawdata.resetPosition(loc)         
      
      #try for addr comment: push 1 byte for the key, 20 for the hash160, 2 for the N and N for the comment 
      try:
         rawdata.advance(1)         
         hash160 = rawdata.get(BINARY_CHUNK, 20)
         chunk_length = rawdata.get(UINT16)
         chunk = rawdata.get(BINARY_CHUNK, chunk_length)
         
         #test the next entry
         dtype, hash, chunk2 = PyBtcWallet().unpackNextEntry(rawdata)
         if dtype>-1 and dtype<5:
            #good entry, return it
            return 1, hash160, chunk, [1, loc]
         else:
            rawdata.resetPosition(loc)
      except: 
         rawdata.resetPosition(loc)   
         
      #try for txn comment: push 1 byte for the key, 32 for the txnhash, 2 for N, and N for the comment
      try:
         rawdata.advance(1)         
         hash256 = rawdata.get(BINARY_CHUNK, 32)
         chunk_length = rawdata.get(UINT16)
         chunk = rawdata.get(BINARY_CHUNK, chunk_length)
         
         #test the next entry
         dtype, hash, chunk2 = PyBtcWallet().unpackNextEntry(rawdata)
         if dtype>-1 and dtype<5:
            #good entry, return it
            return 2, hash256, chunk, [1, loc]
         else:
            rawdata.resetPosition(loc)
      except: 
         rawdata.resetPosition(loc)
         
      #try for deleted entry: 1 byte for the key, 2 bytes for N, N bytes worth of 0s
      try:
         rawdata.advance(1)         
         chunk_length = rawdata.get(UINT16)
         chunk = rawdata.get(BINARY_CHUNK, chunk_length)
         
         #test the next entry
         dtype, hash, chunk2 = PyBtcWallet().unpackNextEntry(rawdata)
         if dtype>-1 and dtype<5:
            baddata = 0
            for i in len(chunk):
               if i != 0:
                  baddata = 1
                  break
               
            if baddata != 0:
               return 4, None, chunk, [1, loc]
         
         rawdata.resetPosition(loc)
      except: 
         rawdata.resetPosition(loc)       
         
      #couldn't find any valid entries, push loc by 1 and try again
      loc = loc +1
      return self.LookForFurtherEntry(rawdata, loc)
   
   #############################################################################   
   def addrEntry_unserialize_recover(self, toUnpack):
      """
      Unserialze a raw address entry, test all checksum carrying members
      
      On errors, flags chksumError bits as follows:
         
         bit 0: addrStr20 error
         
         bit 1: private key error
         bit 2: contains a valid private key even though containsPrivKey is 0
         
         bit 3: iv error
         bit 4: contains a valid iv even though useEncryption is 0
         
         bit 5: pubkey error
         bit 6: contains a valid pubkey even though containsPubKey is 0
         
         bit 7: chaincode error
      """

      if isinstance(toUnpack, BinaryUnpacker):
         serializedData = toUnpack
      else:
         serializedData = BinaryUnpacker( toUnpack )


      def chkzero(a):
         """
         Due to fixed-width fields, we will get lots of zero-bytes
         even when the binary data container was empty
         """
         if a.count('\x00')==len(a):
            return ''
         else:
            return a

      chksumError = 0

      # Start with a fresh new address
      retAddr = PyBtcAddress()

      retAddr.addrStr20 = serializedData.get(BINARY_CHUNK, 20)
      chkAddr20      = serializedData.get(BINARY_CHUNK,  4)

      addrVerInt     = serializedData.get(UINT32)
      flags          = serializedData.get(UINT64)
      retAddr.addrStr20 = verifyChecksum(self.addrStr20, chkAddr20)
      flags = int_to_bitset(flags, widthBytes=8)

      # Interpret the flags
      containsPrivKey              = (flags[0]=='1')
      containsPubKey               = (flags[1]=='1')
      retAddr.useEncryption           = (flags[2]=='1')
      retAddr.createPrivKeyNextUnlock = (flags[3]=='1')

      if len(self.addrStr20)==0:
         chksumError |= 1



      # Write out address-chaining parameters (for deterministic wallets)
      retAddr.chaincode   = chkzero(serializedData.get(BINARY_CHUNK, 32))
      chkChaincode        =         serializedData.get(BINARY_CHUNK,  4)
      retAddr.chainIndex  =         serializedData.get(INT64)
      depth               =         serializedData.get(INT64)
      retAddr.createPrivKeyNextUnlock_ChainDepth = depth

      # Correct errors, convert to secure container
      retAddr.chaincode = SecureBinaryData(verifyChecksum(retAddr.chaincode, chkChaincode))
      if retAddr.chaincode.getSize == 0:
         chksumError |= 128


      # Write out whatever is appropriate for private-key data
      # Binary-unpacker will write all 0x00 bytes if empty values are given
      iv      = chkzero(serializedData.get(BINARY_CHUNK, 16))
      chkIv   =         serializedData.get(BINARY_CHUNK,  4)
      privKey = chkzero(serializedData.get(BINARY_CHUNK, 32))
      chkPriv =         serializedData.get(BINARY_CHUNK,  4)
      iv      = SecureBinaryData(verifyChecksum(iv, chkIv))
      privKey = SecureBinaryData(verifyChecksum(privKey, chkPriv))


      # If this is SUPPOSED to contain a private key...
      if containsPrivKey:
         if privKey.getSize()==0:
            chksumError |= 2
            containsPrivKey = 0 
      else:
         if privKey.getSize()==32:
            chksumError |= 4
            containsPrivKey = 1

      if retAddr.useEncryption:
         if iv.getSize()==0:
            chksumError |= 8
            retAddr.useEncryption = 0
      else:
         if iv.getSize()==16:
            chksumError |= 16
            retAddr.useEncryption = 1
            
      if retAddr.useEncryption:  
         if retAddr.createPrivKeyNextUnlock:
            retAddr.createPrivKeyNextUnlock_IVandKey[0] = iv.copy()
            retAddr.createPrivKeyNextUnlock_IVandKey[1] = privKey.copy()
         else:
            retAddr.binInitVect16     = iv.copy()
            retAddr.binPrivKey32_Encr = privKey.copy()
      else:
         retAddr.binInitVect16      = iv.copy()
         retAddr.binPrivKey32_Plain = privKey.copy()

      pubKey = chkzero(serializedData.get(BINARY_CHUNK, 65))
      chkPub =         serializedData.get(BINARY_CHUNK, 4)
      pubKey = SecureBinaryData(verifyChecksum(pubKey, chkPub))

      if containsPubKey:
         if not pubKey.getSize()==65:
            chksumError |= 32
            if retAddr.binPrivKey32_Plain.getSize()==32:
               pubKey = CryptoAES().ComputePublicKey(retAddr.binPrivKey32_Plain)
      else:
         if pubKey.getSize()==65:
            chksumError |= 64

      retAddr.binPublicKey65 = pubKey

      retAddr.timeRange[0] = serializedData.get(UINT64)
      retAddr.timeRange[1] = serializedData.get(UINT64)
      retAddr.blkRange[0]  = serializedData.get(UINT32)
      retAddr.blkRange[1]  = serializedData.get(UINT32)

      retAddr.isInitialized = True
      
      if (chksumError and 171) == 171:
         raise InvalidEntry
      
      if chksumError != 0:
         #write out errors to the list
         self.rawError.append('   Encountered checksum errors in follolwing address entry members:')
                 
         if chksumError and 1:
            self.rawError.append('      - addrStr20')
         if chksumError and 2: 
            self.rawError.append('      - private key')
         if chksumError and 4:
            self.rawError.append('      - hasPrivatKey flag')
         if chksumError and 8:
            self.rawError.append('      - Encryption IV')
         if chksumError and 16:
            self.rawError.append('      - useEncryption flag')
         if chksumError and 32:
            self.rawError.append('      - public key')
         if chksumError and 64:
            self.rawError.append('      - hasPublicKey flag')
         if chksumError and 128:
            self.rawError.append('      - chaincode')
      
      return retAddr, chksumError

         
         
   #############################################################################
   #GUI related members start here
   #############################################################################
   #############################################################################
   def UIRecoverWallet(self, parentWnd=None):
      """
      Prompts the user with a window asking for wallet path and recovery mode.
      Proceeds to Recover the wallet. Prompt for password if the wallet is locked
      """
      self.parent = parentWnd
      dlg = DlgWltRecoverWallet()
      if dlg.exec_():
         path = str(dlg.edtWalletPath.text())
         mode = 'Bare'
         if dlg.rdbtnStripped.isChecked() is True:
            mode = 'Stripped'
         elif dlg.rdbtnFull.isChecked() is True:
            mode = 'Full'
         elif dlg.rdbtnCheck.isChecked() is True:
            mode = 'Check'
         
         self.RecoverWallet(WalletPath=path, Mode=mode, GUI=True)
      else:
         return False
      
   #############################################################################
   def UIProgress(self):
      self.ProgDlg = DlgProgress(main=self.parent, parent=self.parent, Interrupt="Stop Recovery", Title="<b>Recovering Wallet</b>", TProgress="")
      self.ProgressRdy = 1
      
      self.ProgDlg.spawn_()

 
"""
TODO: setup an array of tests:
1) Gaps in chained address entries
2) broken header
3) oversized comment entries
4) comments for non existant addr or txn entries
5) broken private keys, both imported and chained
6) missing private keys with gaps in chain

possible wallet corruption vectors:
1) PyBtcAddress.unlock verifies consistency between private and public key, unless SkipCheck is forced to false. Look for this scenario
2) Imported private keys: is it possible to import private keys to a locked wallet?
3) What happens when an imported private key is sneaked in between a batch of chain addresses? What if some of the private keys aren't computed yet?
"""

#testing it
#rcwallet = PyBtcWalletRecovery()
#rcwallet.RecoverWallet('/home/goat/Documents/code n shit/watchonly_online_wallet.wallet', 'tests', Mode='Full')
#rcwallet.RecoverWallet('/home/goat/Documents/code n shit/armory_2xCsrj61m_.watchonly.restored_from_paper.wallet', 'tests', Mode='Full')