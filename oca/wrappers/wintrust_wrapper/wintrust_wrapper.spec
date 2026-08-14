@ stdcall AddPersonalTrustDBPages(ptr ptr ptr)
@ stdcall CatalogCompactHashDatabase(wstr wstr wstr wstr)
#@ stub ComputeFirstPageHash
#@ stub ConfigCiFinalPolicy
#@ stub ConfigCiPackageFamilyNameCheck
; @ stdcall CryptCATAdminAcquireContext(ptr ptr long)
; @ stdcall CryptCATAdminAddCatalog(long wstr wstr long)
; @ stdcall CryptCATAdminCalcHashFromFileHandle(long ptr ptr long)
; @ stdcall CryptCATAdminEnumCatalogFromHash(long ptr long long ptr)
; @ stdcall CryptCATAdminPauseServiceForBackup(long long)
; @ stdcall CryptCATAdminReleaseCatalogContext(long long long)
; @ stdcall CryptCATAdminReleaseContext(long long)
; @ stdcall CryptCATAdminRemoveCatalog(ptr wstr long)
; @ stdcall CryptCATAdminResolveCatalogPath(ptr wstr ptr long)
; @ stdcall CryptCATCDFClose(ptr)
; @ stdcall CryptCATCDFEnumAttributes(ptr ptr ptr ptr)
; @ stdcall CryptCATCDFEnumAttributesWithCDFTag(ptr wstr ptr ptr ptr)
; @ stdcall CryptCATCDFEnumCatAttributes(ptr ptr ptr)
; @ stdcall CryptCATCDFEnumMembers(ptr ptr ptr)
; @ stdcall CryptCATCDFEnumMembersByCDFTag(ptr wstr ptr ptr)
; @ stdcall CryptCATCDFEnumMembersByCDFTagEx(ptr wstr ptr ptr long ptr)
; @ stdcall CryptCATCDFOpen(wstr ptr)
; @ stdcall CryptCATCatalogInfoFromContext(ptr ptr long)
; @ stdcall CryptCATClose(long)
; @ stdcall CryptCATEnumerateAttr(ptr ptr ptr)
; @ stdcall CryptCATEnumerateCatAttr(ptr ptr)
; @ stdcall CryptCATEnumerateMember(long ptr)
; @ stdcall CryptCATGetAttrInfo(ptr ptr wstr)
; @ stdcall CryptCATGetCatAttrInfo(ptr wstr )
; @ stdcall CryptCATGetMemberInfo(ptr wstr)
; @ stdcall CryptCATHandleFromStore(ptr)
; @ stdcall CryptCATOpen(wstr long long long long)
; @ stdcall CryptCATPersistStore(ptr)
; @ stdcall CryptCATPutAttrInfo(ptr ptr wstr long long ptr)
; @ stdcall CryptCATPutCatAttrInfo(ptr wstr long long ptr)
; @ stdcall CryptCATPutMemberInfo(ptr wstr wstr ptr long long ptr)
; @ stdcall CryptCATStoreFromHandle(ptr)
; @ stdcall CryptCATVerifyMember(ptr ptr ptr)
@ stdcall CryptSIPCreateIndirectData(ptr ptr ptr)
@ stdcall CryptSIPGetInfo(ptr)
@ stdcall CryptSIPGetRegWorkingFlags(ptr)
#@ stub CryptSIPGetSealedDigest
@ stdcall CryptSIPGetSignedDataMsg(ptr ptr long ptr ptr)
@ stdcall CryptSIPPutSignedDataMsg(ptr long ptr long ptr)
@ stdcall CryptSIPRemoveSignedDataMsg(ptr long)
@ stdcall CryptSIPVerifyIndirectData(ptr ptr)
@ stdcall DllRegisterServer()
@ stdcall DllUnregisterServer()
@ stdcall DriverCleanupPolicy(ptr)
@ stdcall DriverFinalPolicy(ptr)
@ stdcall DriverInitializePolicy(ptr)
@ stdcall FindCertsByIssuer(ptr ptr ptr ptr long wstr long)
@ stdcall GenericChainCertificateTrust(ptr)
@ stdcall GenericChainFinalProv(ptr)
#@ stub GetAuthenticodeSha256Hash
@ stdcall HTTPSCertificateTrust(ptr)
@ stdcall HTTPSFinalProv(ptr)
@ stdcall IsCatalogFile(ptr wstr)
@ stdcall MsCatConstructHashTag(long ptr wstr)
@ stdcall MsCatFreeHashTag(wstr)
@ stdcall OfficeCleanupPolicy(ptr)
@ stdcall OfficeInitializePolicy(ptr)
@ stdcall OpenPersonalTrustDBDialog(ptr)
#@ stub OpenPersonalTrustDBDialogEx
@ stdcall SoftpubAuthenticode(ptr)
@ stdcall SoftpubCheckCert(ptr long long long)
@ stdcall SoftpubCleanup(ptr)
@ stdcall SoftpubDefCertInit(ptr)
@ stdcall SoftpubDllRegisterServer()
@ stdcall SoftpubDllUnregisterServer()
@ stdcall SoftpubDumpStructure(ptr)
@ stdcall SoftpubFreeDefUsageCallData(str ptr)
@ stdcall SoftpubInitialize(ptr)
@ stdcall SoftpubLoadDefUsageCallData(str ptr)
@ stdcall SoftpubLoadMessage(ptr)
@ stdcall SoftpubLoadSignature(ptr)
#@ stub SrpCheckSmartlockerEAandProcessToken
@ stdcall TrustDecode(long ptr ptr long long str ptr long long)
@ stdcall TrustFindIssuerCertificate(ptr long long ptr ptr ptr ptr )
@ stdcall TrustFreeDecode(ptr long)
@ stdcall TrustIsCertificateSelfSigned(ptr)
@ stdcall TrustOpenStores(ptr ptr ptr long)
#@ stub WTGetBioSignatureInfo
#@ stub WTGetPluginSignatureInfo
#@ stub WTGetSignatureInfo
@ stdcall WTHelperCertCheckValidSignature(ptr)
@ stdcall WTHelperCertFindIssuerCertificate(ptr long ptr ptr long ptr ptr)
@ stdcall WTHelperCertIsSelfSigned(long ptr)
@ stdcall WTHelperCheckCertUsage(ptr str)
@ stdcall WTHelperGetAgencyInfo(ptr ptr ptr)
@ stdcall WTHelperGetFileHandle(ptr)
@ stdcall WTHelperGetFileHash(wstr long ptr ptr ptr ptr)
@ stdcall WTHelperGetFileName(ptr)
@ stdcall WTHelperGetKnownUsages(long ptr)
@ stdcall WTHelperGetProvCertFromChain(ptr long)
@ stdcall WTHelperGetProvPrivateDataFromChain(ptr ptr)
@ stdcall WTHelperGetProvSignerFromChain(ptr long long long)
#@ stub WTHelperIsChainedToMicrosoft
#@ stub WTHelperIsChainedToMicrosoftFromStateData
@ stdcall WTHelperIsInRootStore(ptr ptr)
@ stdcall WTHelperOpenKnownStores(ptr)
@ stdcall WTHelperProvDataFromStateData(ptr)
#@ stub WTIsFirstConfigCiResultPreferred
#@ stub WTLogConfigCiScriptEvent
#@ stub WTLogConfigCiSignerEvent
#@ stub WTValidateBioSignaturePolicy
@ stdcall WVTAsn1CatMemberInfoDecode(long str ptr long long ptr ptr)
@ stdcall WVTAsn1CatMemberInfoEncode(long str ptr ptr ptr)
@ stdcall WVTAsn1CatNameValueDecode(long str ptr long long ptr ptr)
@ stdcall WVTAsn1CatNameValueEncode(long str ptr ptr ptr)
#@ stub WVTAsn1IntentToSealAttributeDecode
#@ stub WVTAsn1IntentToSealAttributeEncode
#@ stub WVTAsn1SealingSignatureAttributeDecode
#@ stub WVTAsn1SealingSignatureAttributeEncode
#@ stub WVTAsn1SealingTimestampAttributeDecode
#@ stub WVTAsn1SealingTimestampAttributeEncode
@ stdcall WVTAsn1SpcFinancialCriteriaInfoDecode(long str ptr long long ptr ptr)
@ stdcall WVTAsn1SpcFinancialCriteriaInfoEncode(long str ptr ptr ptr)
@ stdcall WVTAsn1SpcIndirectDataContentDecode(long str ptr long long ptr ptr)
@ stdcall WVTAsn1SpcIndirectDataContentEncode(long str ptr ptr ptr)
@ stdcall WVTAsn1SpcLinkDecode(long str ptr long long ptr ptr)
@ stdcall WVTAsn1SpcLinkEncode(long str ptr ptr ptr)
@ stdcall WVTAsn1SpcMinimalCriteriaInfoDecode(long str ptr long long  ptr ptr)
@ stdcall WVTAsn1SpcMinimalCriteriaInfoEncode(long str ptr ptr ptr)
@ stdcall WVTAsn1SpcPeImageDataDecode(long str ptr long long ptr ptr)
@ stdcall WVTAsn1SpcPeImageDataEncode(long str ptr ptr ptr)
@ stdcall WVTAsn1SpcSigInfoDecode(long str ptr long long ptr long)
@ stdcall WVTAsn1SpcSigInfoEncode(long str ptr ptr long)
@ stdcall WVTAsn1SpcSpAgencyInfoDecode(long str ptr long long ptr long)
@ stdcall WVTAsn1SpcSpAgencyInfoEncode(long str ptr ptr ptr)
@ stdcall WVTAsn1SpcSpOpusInfoDecode(long str ptr long long ptr ptr)
@ stdcall WVTAsn1SpcSpOpusInfoEncode(long str ptr ptr ptr)
@ stdcall WVTAsn1SpcStatementTypeDecode(long str ptr long long ptr ptr)
@ stdcall WVTAsn1SpcStatementTypeEncode(long str ptr ptr ptr)
@ stdcall WinVerifyTrust(long ptr ptr)
@ stdcall WinVerifyTrustEx(long ptr ptr)
@ stdcall WintrustAddActionID(ptr long ptr)
@ stdcall WintrustAddDefaultForUsage(ptr ptr)
@ stdcall WintrustCertificateTrust(ptr)
@ stdcall WintrustGetDefaultForUsage(long str ptr)
@ stdcall WintrustGetRegPolicyFlags(ptr)
@ stdcall WintrustLoadFunctionPointers(ptr ptr)
@ stdcall WintrustRemoveActionID(ptr)
@ stdcall WintrustSetRegPolicyFlags(long)
@ stdcall mscat32DllRegisterServer()
@ stdcall mscat32DllUnregisterServer()
@ stdcall mssip32DllRegisterServer()
@ stdcall mssip32DllUnregisterServer()

#WinVista functions
@ stdcall CryptCATAdminAcquireContext2(ptr ptr wstr ptr long) wintrustbase.CryptCATAdminAcquireContext2
@ stdcall CryptCATAdminCalcHashFromFileHandle2(ptr ptr ptr ptr long) wintrustbase.CryptCATAdminCalcHashFromFileHandle2
@ stdcall CryptCATAllocSortedMemberInfo(ptr wstr) wintrustbase.CryptCATAllocSortedMemberInfo
@ stdcall CryptCATFreeSortedMemberInfo(ptr ptr) wintrustbase.CryptCATFreeSortedMemberInfo
@ stdcall CryptSIPGetCaps(ptr ptr) wintrustbase.CryptSIPGetCaps
@ stdcall WintrustSetDefaultIncludePEPageHashes(long) wintrustbase.WintrustSetDefaultIncludePEPageHashes
@ stdcall WVTAsn1CatMemberInfo2Decode(long long ptr long long ptr ptr) wintrustbase.WVTAsn1CatMemberInfo2Decode
@ stdcall WVTAsn1CatMemberInfo2Encode(long long ptr ptr ptr) wintrustbase.WVTAsn1CatMemberInfo2Encode