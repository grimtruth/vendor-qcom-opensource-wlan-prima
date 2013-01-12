/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file limAssocUtils.cc contains the utility functions
 * LIM uses while processing (Re) Association messages.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 * 05/26/10       js             WPA handling in (Re)Assoc frames
 * 
 */
 
#include "palTypes.h"
#include "aniGlobal.h"
#include "wniApi.h"
#include "sirCommon.h"

#if (WNI_POLARIS_FW_PRODUCT == AP)
#include "wniCfgAp.h"
#else
#include "wniCfgSta.h"
#endif
#include "pmmApi.h"
#include "cfgApi.h"

#include "schApi.h"
#include "utilsApi.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limSecurityUtils.h"
#include "limSerDesUtils.h"
#include "limStaHashApi.h"
#include "limAdmitControl.h"
#include "limSendMessages.h"
#include "limIbssPeerMgmt.h"
#include "limSession.h"

#include "vos_types.h"
#include "wlan_qct_wda.h"

/*
 * fill up the rate info properly based on what is actually supported by the peer
 * TBD TBD TBD
 */
void
limFillSupportedRatesInfo(
    tpAniSirGlobal          pMac,
    tpDphHashNode           pSta,
    tpSirSupportedRates   pRates,
    tpPESession           psessionEntry)
{
    //pSta will be NULL for self entry, so get the opRateMode based on the self mode.
    //For the peer entry get it from the peer Capabilities present in hash table
    if(pSta == NULL)
        pRates->opRateMode = limGetStaRateMode((tANI_U8)psessionEntry->dot11mode);
    else
        pRates->opRateMode = limGetStaPeerType(pMac, pSta, psessionEntry);

}


/**
 * limCmpSSid()
 *
 *FUNCTION:
 * This function is called in various places within LIM code
 * to determine whether received SSid is same as SSID in use.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  *prxSSid - pointer to SSID structure
 *
 * @return status - true for SSID match else false.
 */

tANI_U8
limCmpSSid(tpAniSirGlobal pMac, tSirMacSSid *prxSSid,tpPESession psessionEntry)
{

    if( palEqualMemory( pMac->hHdd,  (tANI_U8* ) prxSSid, (tANI_U8 *) &psessionEntry->ssId, 
                                    (tANI_U8) (psessionEntry->ssId.length + 1)))
        return true;
    else
        return false;

} /****** end limCmpSSid() ******/



/**
 * limCompareCapabilities()
 *
 *FUNCTION:
 * This function is called during Association/Reassociation
 * frame handling to determine whether received capabilities
 * match with local capabilities or not.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac         - Pointer to Global MAC structure
 * @param  pAssocReq    - Pointer to received Assoc Req frame
 * @param  pLocalCapabs - Pointer to local capabilities
 *
 * @return status - true for Capabilitity match else false.
 */

tANI_U8
limCompareCapabilities(tpAniSirGlobal pMac,
                       tSirAssocReq *pAssocReq,
                       tSirMacCapabilityInfo *pLocalCapabs,tpPESession psessionEntry)
{
    tANI_U32   val;


    if ( ((psessionEntry->limSystemRole == eLIM_AP_ROLE)||
         (psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE)) &&
         (pAssocReq->capabilityInfo.ibss) )
    {
        // Requesting STA asserting IBSS capability.
        return false;
    }

    // Compare CF capabilities
    if (pAssocReq->capabilityInfo.cfPollable ||
        pAssocReq->capabilityInfo.cfPollReq)
    {
        // AP does not support PCF functionality
        return false;
    }

#if 0    //See CR24696 for analysis
    // Compare privacy capability
    if (pAssocReq->capabilityInfo.privacy != pLocalCapabs->privacy)
    {
        // AP does not support privacy
        return false;
    }
#endif
    
    // Compare short preamble capability
    if (pAssocReq->capabilityInfo.shortPreamble &&
        (pAssocReq->capabilityInfo.shortPreamble !=
         pLocalCapabs->shortPreamble))
    {
        PELOG1(limLog(pMac, LOG1,
           FL("Allowing a STA requesting short preamble while AP does not support it\n"));)
#if 0
        // AP does not support short preamable
        return false;
#endif
    }


    limLog(pMac, LOGW, "QoS in AssocReq: %d, local ShortP: %d\n", 
              pAssocReq->capabilityInfo.qos,
              pLocalCapabs->qos);

    // Compare QoS capability
    if (pAssocReq->capabilityInfo.qos &&
        (pAssocReq->capabilityInfo.qos != pLocalCapabs->qos))
    {
        /*Temporary hack for UPF to skip 11e capability check in order to interop with 
          CSR - proper fix needs to be put in place*/
        if ( 0 != vos_get_skip_11e_check())
        {
             limLog(pMac, LOG1, FL("Received unmatched QOS but cfg to suppress - continuing\n"));
        }
        else
        {
        // AP does not support QoS capability
        return false;
        }
    }


    /* 
     * If AP supports shortSlot and if apple user has
     * enforced association only from shortSlot station,
     * then AP must reject any station that does not support
     * shortSlot
     */
    if ( ((psessionEntry->limSystemRole == eLIM_AP_ROLE)||(psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE)) && (pLocalCapabs->shortSlotTime == 1) )

    {
        if (wlan_cfgGetInt(pMac, WNI_CFG_ACCEPT_SHORT_SLOT_ASSOC_ONLY, &val) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP, FL("error getting WNI_CFG_FORCE_SHORT_SLOT_ASSOC_ONLY \n"));
            return false;
        }
        if(val)
        {
            if (pAssocReq->capabilityInfo.shortSlotTime != pLocalCapabs->shortSlotTime)
            return false;
        }
    }
            
    return true;
} /****** end limCompareCapabilities() ******/


/**
 * limCheckRxBasicRates()
 *
 *FUNCTION:
 * This function is called during Association/Reassociation
 * frame handling to determine whether received rates in
 * Assoc/Reassoc request frames include all BSS basic rates
 * or not.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  rxRateSet - pointer to SSID structure
 *
 * @return status - true if ALL BSS basic rates are present in the
 *                  received rateset else false.
 */

tANI_U8
limCheckRxBasicRates(tpAniSirGlobal pMac, tSirMacRateSet rxRateSet,tpPESession psessionEntry)
{
    tSirMacRateSet    *pRateSet, basicRate;
    tANI_U8    i, j, k, match;

    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pRateSet, sizeof(tSirMacRateSet)))
    {
        // Log error
        limLog(pMac, LOGP, FL("call to palAllocateMemory failed for RATESET\n"));

        return false;
    }

    
    #if 0
    if (wlan_cfgGetStr(pMac, WNI_CFG_OPERATIONAL_RATE_SET,
                  (tANI_U8 *) &pRateSet->rate,
                  (tANI_U32 *) &cfgLen) != eSIR_SUCCESS)
    {
        /// Could not get Operational rateset from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve Operational rateset\n"));

        // Free up memory allocated for rateset
        palFreeMemory( pMac->hHdd, (tANI_U8 *) pRateSet);

        return false;
    }
    #endif //TO SUPPORT BT-AMP

    /* Copy operational rate set from session Entry */
    palCopyMemory(pMac->hHdd,  pRateSet->rate,  (psessionEntry->rateSet.rate),  psessionEntry->rateSet.numRates);
    
    pRateSet->numRates = psessionEntry->rateSet.numRates;

    // Extract BSS basic rateset from operational rateset
    for (i = 0, j = 0; i < pRateSet->numRates; i++)
    {
        if ((pRateSet->rate[i] & 0x80) == 0x80)
        {
            // msb is set, so this is a basic rate
            basicRate.rate[j++] = pRateSet->rate[i];
        }
    }

    /*
     * For each BSS basic rate, find if it is present in the
     * received rateset.
     */
    for (k = 0; k < j; k++)
    {
        match = 0;
        for (i = 0; i < rxRateSet.numRates; i++)
        {
            if ((rxRateSet.rate[i] | 0x80) ==    basicRate.rate[k])
                match = 1;
        }

        if (!match)
        {
            // Free up memory allocated for rateset
            palFreeMemory( pMac->hHdd, (tANI_U8 *) pRateSet);

            return false;
        }
    }

    // Free up memory allocated for rateset
    palFreeMemory( pMac->hHdd, (tANI_U8 *) pRateSet);

    return true;
} /****** end limCheckRxBasicRates() ******/



/**
 * limCheckMCSSet()
 *
 *FUNCTION:
 * This function is called during Association/Reassociation
 * frame handling to determine whether received MCS rates in
 * Assoc/Reassoc request frames includes all Basic MCS Rate Set or not.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  supportedMCSSet - pointer to Supported MCS Rate Set
 *
 * @return status - true if ALL MCS Basic Rate Set rates are present in the
 *                  received rateset else false.
 */

tANI_U8
limCheckMCSSet(tpAniSirGlobal pMac, tANI_U8* supportedMCSSet)
{
    tANI_U8 basicMCSSet[SIZE_OF_BASIC_MCS_SET] = {0};
    tANI_U32   cfgLen = 0;
    tANI_U8 i;
    tANI_U8 validBytes;
    tANI_U8 lastByteMCSMask = 0x1f;


    cfgLen = WNI_CFG_BASIC_MCS_SET_LEN;
    if (wlan_cfgGetStr(pMac, WNI_CFG_BASIC_MCS_SET,
                  (tANI_U8 *) basicMCSSet,
                  (tANI_U32 *) &cfgLen) != eSIR_SUCCESS)
    {
        /// Could not get Basic MCS rateset from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve Basic MCS rateset\n"));
        return false;
    }

    validBytes = VALID_MCS_SIZE/8;

    //check if all the Basic MCS Bits are set in supported MCS bitmap
    for(i=0; i<validBytes; i++)
    {
        if( (basicMCSSet[i] & supportedMCSSet[i]) != basicMCSSet[i])
            {
                PELOGW(limLog(pMac, LOGW, FL("One of Basic MCS Set Rates is not supported by the Station."));)
                return false;
            }
    }

    //check the last 5 bits of the valid MCS bitmap
    if( ((basicMCSSet[i] & lastByteMCSMask) & (supportedMCSSet[i] & lastByteMCSMask)) !=
          (basicMCSSet[i] & lastByteMCSMask))
        {
            PELOGW(limLog(pMac, LOGW, FL("One of Basic MCS Set Rates is not supported by the Station."));)
            return false;
        }

    return true;
}

#ifdef WLAN_SOFTAP_FEATURE

#define SECURITY_SUITE_TYPE_MASK 0xFF
#define SECURITY_SUITE_TYPE_WEP40 0x1
#define SECURITY_SUITE_TYPE_TKIP 0x2
#define SECURITY_SUITE_TYPE_CCMP 0x4
#define SECURITY_SUITE_TYPE_WEP104 0x4

/**
 * limCheckRxRSNIeMatch()
 *
 *FUNCTION:
 * This function is called during Association/Reassociation
 * frame handling to determine whether received RSN in
 * Assoc/Reassoc request frames include supported cipher suites or not.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  rxRSNIe - received RSN IE in (Re)Assco req
 *
 * @return status - true if ALL BSS basic rates are present in the
 *                  received rateset else false.
 */

tANI_U8
limCheckRxRSNIeMatch(tpAniSirGlobal pMac, tDot11fIERSN rxRSNIe,tpPESession pSessionEntry, tANI_U8 staIsHT)
{
    tDot11fIERSN    *pRSNIe;
    tANI_U8         i, j, match, onlyNonHtCipher = 1;


    //RSN IE should be received from PE
    pRSNIe = &pSessionEntry->gStartBssRSNIe;

    // Check groupwise cipher suite
    for (i = 0; i < sizeof(rxRSNIe.gp_cipher_suite); i++)
    {
        if (pRSNIe->gp_cipher_suite[i] != rxRSNIe.gp_cipher_suite[i])
        {
            return eSIR_MAC_INVALID_GROUP_CIPHER_STATUS;
        }
    }

    /*
     * For each Pairwise cipher suite check whether we support
     * received pairwise
     */
    match = 0;
    for (i = 0; i < rxRSNIe.pwise_cipher_suite_count; i++)
    {
        for(j = 0; j < pRSNIe->pwise_cipher_suite_count; j++)
        {
            if(palEqualMemory(pMac,&rxRSNIe.pwise_cipher_suites[i],
                                  &pRSNIe->pwise_cipher_suites[j],
                                  sizeof(pRSNIe->pwise_cipher_suites[j])))
            {
                match = 1;
                break;
            }
        }

        if ((staIsHT) 
#ifdef ANI_LITTLE_BYTE_ENDIAN
            &&( (rxRSNIe.pwise_cipher_suites[i][3] & SECURITY_SUITE_TYPE_MASK) == SECURITY_SUITE_TYPE_CCMP))
#else
            &&( (rxRSNIe.pwise_cipher_suites[i][0] & SECURITY_SUITE_TYPE_MASK) == SECURITY_SUITE_TYPE_CCMP))
#endif            
        {
             onlyNonHtCipher=0;
        }

    }

    if ((!match) || ((staIsHT) && onlyNonHtCipher))
    {
        return eSIR_MAC_INVALID_PAIRWISE_CIPHER_STATUS;
    }
    /* Check RSN capabilities */
    if(rxRSNIe.preauth == true) //this is supported by AP only
    {
        return eSIR_MAC_INVALID_RSN_IE_CAPABILITIES_STATUS;
    }

    return eSIR_SUCCESS;
} /****** end limCheckRxRSNIeMatch() ******/

/**
 * limCheckRxWPAIeMatch()
 *
 *FUNCTION:
 * This function is called during Association/Reassociation
 * frame handling to determine whether received RSN in
 * Assoc/Reassoc request frames include supported cipher suites or not.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  rxWPAIe - Received WPA IE in (Re)Assco req
 *
 * @return status - true if ALL BSS basic rates are present in the
 *                  received rateset else false.
 */

tANI_U8
limCheckRxWPAIeMatch(tpAniSirGlobal pMac, tDot11fIEWPA rxWPAIe,tpPESession pSessionEntry, tANI_U8 staIsHT)
{
    tDot11fIEWPA    *pWPAIe;
    tANI_U8         i, j, match, onlyNonHtCipher = 1;

    // WPA IE should be received from PE
    pWPAIe = &pSessionEntry->gStartBssWPAIe;

    // Check groupwise cipher suite
    for (i = 0; i < 4; i++)
    {
        if (pWPAIe->multicast_cipher[i] != rxWPAIe.multicast_cipher[i])
        {
            return eSIR_MAC_INVALID_GROUP_CIPHER_STATUS;
        }
    }

    /*
     * For each Pairwise cipher suite check whether we support
     * received pairwise
     */
    match = 0;
    for (i = 0; i < rxWPAIe.unicast_cipher_count; i++)
    {
        for(j = 0; j < pWPAIe->unicast_cipher_count; j++)
        {
            if(palEqualMemory(pMac,rxWPAIe.unicast_ciphers[i],
                                  pWPAIe->unicast_ciphers[j],
                                  4))
            {
                match = 1;
                break;
            }
        }

        if ((staIsHT) 
#ifdef ANI_LITTLE_BYTE_ENDIAN
            &&( (rxWPAIe.unicast_ciphers[i][3] & SECURITY_SUITE_TYPE_MASK) == SECURITY_SUITE_TYPE_CCMP))
#else
            &&( (rxWPAIe.unicast_ciphers[i][0] & SECURITY_SUITE_TYPE_MASK) == SECURITY_SUITE_TYPE_CCMP))
#endif            
        {
             onlyNonHtCipher=0;
        }

    }

    if ((!match) || ((staIsHT) && onlyNonHtCipher))
    {
        return eSIR_MAC_CIPHER_SUITE_REJECTED_STATUS;
    }

    return eSIR_SUCCESS;
} /****** end limCheckRxWPAIeMatch() ******/

#endif /* WLAN_SOFTAP_FEATURE */

/**
 * limCleanupRxPath()
 *
 *FUNCTION:
 * This function is called to cleanup STA state at SP & RFP.
 *
 *LOGIC:
 * To circumvent RFP's handling of dummy packet when it does not
 * have an incomplete packet for the STA to be deleted, a packet
 * with 'more framgents' bit set will be queued to RFP's WQ before
 * queuing 'dummy packet'.
 * A 'dummy' BD is pushed into RFP's WQ with type=00, subtype=1010
 * (Disassociation frame) and routing flags in BD set to eCPU's
 * Low Priority WQ.
 * RFP cleans up its local context for the STA id mentioned in the
 * BD and then pushes BD to eCPU's low priority WQ.
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac    Pointer to Global MAC structure
 * @param pStaDs  Pointer to the per STA data structure
 *                initialized by LIM and maintained at DPH
 *
 * @return None
 */

tSirRetStatus
limCleanupRxPath(tpAniSirGlobal pMac, tpDphHashNode pStaDs,tpPESession psessionEntry)
{
    tSirRetStatus       retCode = eSIR_SUCCESS;


    PELOG2(limLog( pMac, LOG2, FL("**Initiate cleanup"));)

    limAbortBackgroundScan( pMac );

    if (pMac->lim.gLimAddtsSent)
    {
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_DEACTIVATE, psessionEntry->peSessionId, eLIM_ADDTS_RSP_TIMER));
        tx_timer_deactivate(&pMac->lim.limTimers.gLimAddtsRspTimer);
    }

    if (pStaDs->mlmStaContext.mlmState == eLIM_MLM_WT_ASSOC_CNF_STATE)
    {
        limDeactivateAndChangePerStaIdTimer(pMac, eLIM_CNF_WAIT_TIMER,
                                                pStaDs->assocId);

        if (!pStaDs->mlmStaContext.updateContext)
        {
            /**
             * There is no context at Polaris to delete.
             * Release our assigned AID back to the free pool
             */
            if ((psessionEntry->limSystemRole == eLIM_AP_ROLE) || 
                (psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE))
            {    
                limReleasePeerIdx(pMac, pStaDs->assocId, psessionEntry);
            }
            limDeleteDphHashEntry(pMac, pStaDs->staAddr, pStaDs->assocId,psessionEntry);

            return retCode;
        }
    }

    //delete all tspecs associated with this sta.
    limAdmitControlDeleteSta(pMac, pStaDs->assocId);

#if (WNI_POLARIS_FW_PRODUCT==AP)
    // Reset PMM state for this STA if it exists
    if (psessionEntry->limSystemRole == eLIM_AP_ROLE)||(psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE)
        pmmUpdatePMMode(pMac, pStaDs->assocId, 0);
#endif

    /**
     * Make STA hash entry invalid at eCPU so that DPH
     * does not process any more data packets and
     * releases those BDs
     */
    pStaDs->valid                    = 0;
    pStaDs->mlmStaContext.mlmState   = eLIM_MLM_WT_DEL_STA_RSP_STATE;

    if ((psessionEntry->limSystemRole == eLIM_STA_ROLE)||(psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE))
    {
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, eLIM_MLM_WT_DEL_STA_RSP_STATE));
        psessionEntry->limMlmState = eLIM_MLM_WT_DEL_STA_RSP_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, eLIM_MLM_WT_DEL_STA_RSP_STATE));
        /* Deactivating probe after heart beat timer */
        limDeactivateAndChangeTimer(pMac, eLIM_PROBE_AFTER_HB_TIMER);
        limHeartBeatDeactivateAndChangeTimer(pMac, psessionEntry);
        limDeactivateAndChangeTimer(pMac, eLIM_KEEPALIVE_TIMER);
        pMac->lim.gLastBeaconDtimCount = 0;
        pMac->lim.gLastBeaconDtimPeriod = 0;

#ifdef FEATURE_WLAN_CCX
        limDeactivateAndChangeTimer(pMac,eLIM_TSM_TIMER);
#endif

        /**
         * Update the status for PMM module
         */
        pmmResetPmmState(pMac);
    }
#ifdef WLAN_DEBUG    
    // increment a debug count
    pMac->lim.gLimNumRxCleanup++;
#endif

    if (psessionEntry->limSmeState == eLIM_SME_JOIN_FAILURE_STATE) {
        retCode = limDelBss( pMac, pStaDs, psessionEntry->bssIdx, psessionEntry);
    }
    else
        retCode = limDelSta( pMac, pStaDs, true, psessionEntry);

    return retCode;

} /*** end limCleanupRxPath() ***/


/**
 * limSendDelStaCnf()
 *
 *FUNCTION:
 * This function is called to  send appropriate CNF message to SME
 *
 *LOGIC:
 *
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac    Pointer to Global MAC structure
 * @param tpAniSirGlobal pMac,
 * @param tSirMacAddr staDsAddr,
 * @param tANI_U16 staDsAssocId,
 * @param tLimMlmStaContext mlmStaContext,
 * @param tSirResultCodes statusCode
 *
 * @return None
 */

void
limSendDelStaCnf(tpAniSirGlobal pMac, tSirMacAddr staDsAddr,
       tANI_U16 staDsAssocId, tLimMlmStaContext mlmStaContext, tSirResultCodes statusCode,tpPESession psessionEntry)
{

    tLimMlmDisassocCnf mlmDisassocCnf;
    tLimMlmDeauthCnf   mlmDeauthCnf;
    tLimMlmPurgeStaInd mlmPurgeStaInd;

    if ((psessionEntry->limSystemRole == eLIM_STA_ROLE)||(psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE))
    {
        // Set BSSID at CFG to null
        tSirMacAddr nullAddr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        #if 0
        if (cfgSetStr(pMac, WNI_CFG_BSSID, (tANI_U8 *) &nullAddr,
                      sizeof(tSirMacAddr)) != eSIR_SUCCESS)
        {
            /// Could not update BSSID at CFG. Log error.
            limLog(pMac, LOGP, FL("could not update BSSID at CFG\n"));

            return;
        }
        #endif//TO SUPPORT BT-AMP
        
        sirCopyMacAddr(nullAddr,psessionEntry->bssId);

        // Free up buffer allocated for JoinReq held by
        // MLM state machine
        if (psessionEntry->pLimMlmJoinReq)
        {
            palFreeMemory( pMac->hHdd, (tANI_U8 *) psessionEntry->pLimMlmJoinReq);
            psessionEntry->pLimMlmJoinReq = NULL;
        }

        psessionEntry->limAID = 0;


    }

    if ((mlmStaContext.cleanupTrigger ==
                                      eLIM_HOST_DISASSOC) ||
        (mlmStaContext.cleanupTrigger ==
                                      eLIM_LINK_MONITORING_DISASSOC) ||
        (mlmStaContext.cleanupTrigger ==
                                      eLIM_PROMISCUOUS_MODE_DISASSOC))
    {
        /**
         * Host or LMM driven Disassociation.
         * Issue Disassoc Confirm to SME.
         */
           limLog( pMac, LOGW, FL("Lim Posting DISASSOC_CNF to Sme. Trigger: %X\n"), mlmStaContext.cleanupTrigger);


        palCopyMemory( pMac->hHdd,
                   (tANI_U8 *) &mlmDisassocCnf.peerMacAddr,
                   (tANI_U8 *) staDsAddr,
                   sizeof(tSirMacAddr));
        mlmDisassocCnf.resultCode = statusCode;
#if (WNI_POLARIS_FW_PRODUCT == AP)
        mlmDisassocCnf.aid        = staDsAssocId;
#endif
        mlmDisassocCnf.disassocTrigger =
                                   mlmStaContext.cleanupTrigger;
        /* Update PE session Id*/
        mlmDisassocCnf.sessionId = psessionEntry->peSessionId;

        limPostSmeMessage(pMac,
                          LIM_MLM_DISASSOC_CNF,
                          (tANI_U32 *) &mlmDisassocCnf);
    }
    else if ((mlmStaContext.cleanupTrigger ==
                                           eLIM_HOST_DEAUTH) ||
             (mlmStaContext.cleanupTrigger ==
                                           eLIM_LINK_MONITORING_DEAUTH))
    {
        /**
         * Host or LMM driven Deauthentication.
         * Issue Deauth Confirm to SME.
         */
        limLog( pMac, LOGW, FL("Lim Posting DEAUTH_CNF to Sme. Trigger: %X\n"), mlmStaContext.cleanupTrigger);
        palCopyMemory( pMac->hHdd, (tANI_U8 *) &mlmDeauthCnf.peerMacAddr,
                      (tANI_U8 *) staDsAddr,
                      sizeof(tSirMacAddr));
        mlmDeauthCnf.resultCode    = statusCode;
#if (WNI_POLARIS_FW_PRODUCT == AP)
        mlmDeauthCnf.aid           = staDsAssocId;
#endif
        mlmDeauthCnf.deauthTrigger =
                                   mlmStaContext.cleanupTrigger;
        /* PE session Id */
        mlmDeauthCnf.sessionId = psessionEntry->peSessionId;

        limPostSmeMessage(pMac,
                          LIM_MLM_DEAUTH_CNF,
                          (tANI_U32 *) &mlmDeauthCnf);
    }
    else if ((mlmStaContext.cleanupTrigger ==
                                          eLIM_PEER_ENTITY_DISASSOC) ||
             (mlmStaContext.cleanupTrigger ==
                                           eLIM_PEER_ENTITY_DEAUTH))
    {
        /**
         * Received Disassociation/Deauthentication from peer.
         * Issue Purge Ind to SME.
         */
        limLog( pMac, LOGW, FL("Lim Posting PURGE_STA_IND to Sme. Trigger: %X\n"), mlmStaContext.cleanupTrigger) ;
        palCopyMemory( pMac->hHdd, (tANI_U8 *) &mlmPurgeStaInd.peerMacAddr,
                      (tANI_U8 *) staDsAddr,
                      sizeof(tSirMacAddr));
        mlmPurgeStaInd.reasonCode   = (tANI_U8) mlmStaContext.disassocReason;
        mlmPurgeStaInd.aid          = staDsAssocId;
        mlmPurgeStaInd.purgeTrigger = mlmStaContext.cleanupTrigger;
        mlmPurgeStaInd.sessionId = psessionEntry->peSessionId;

        limPostSmeMessage(pMac,
                          LIM_MLM_PURGE_STA_IND,
                          (tANI_U32 *) &mlmPurgeStaInd);
    }
    else if(mlmStaContext.cleanupTrigger == eLIM_JOIN_FAILURE)
    {
        //PE setup the peer entry in HW upfront, right after join is completed.
        //If there is a failure during rest of the assoc sequence, this context needs to be cleaned up.
        tANI_U8         smesessionId;
        tANI_U16        smetransactionId;

        smesessionId = psessionEntry->smeSessionId;
        smetransactionId = psessionEntry->transactionId;

        psessionEntry->limSmeState = eLIM_SME_JOIN_FAILURE_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, psessionEntry->peSessionId, psessionEntry->limSmeState));

        //if it is a reassoc failure to join new AP
        if((mlmStaContext.resultCode == eSIR_SME_FT_REASSOC_TIMEOUT_FAILURE) ||
           (mlmStaContext.resultCode == eSIR_SME_FT_REASSOC_FAILURE))
        {
            if(mlmStaContext.resultCode != eSIR_SME_SUCCESS )
            {
                peDeleteSession(pMac, psessionEntry);
                psessionEntry = NULL;
            } 

            limSendSmeJoinReassocRsp(pMac, eWNI_SME_REASSOC_RSP,
                               mlmStaContext.resultCode, mlmStaContext.protStatusCode, psessionEntry,
                               smesessionId, smetransactionId);
        }
        else
        {
        palFreeMemory( pMac->hHdd, psessionEntry->pLimJoinReq);
        psessionEntry->pLimJoinReq = NULL;

        if(mlmStaContext.resultCode != eSIR_SME_SUCCESS)
        {
            peDeleteSession(pMac,psessionEntry);
            psessionEntry = NULL;
        } 
        
        limSendSmeJoinReassocRsp(pMac, eWNI_SME_JOIN_RSP, mlmStaContext.resultCode, mlmStaContext.protStatusCode,
                                 psessionEntry, smesessionId, smetransactionId);
        }
        
    } 

    if((NULL != psessionEntry)
#ifdef WLAN_SOFTAP_FEATURE
        && (eLIM_AP_ROLE != psessionEntry->limSystemRole )
#endif     
    )      
            {
                peDeleteSession(pMac,psessionEntry);
                psessionEntry = NULL;
            }
}

/**
 * limRejectAssociation()
 *
 *FUNCTION:
 * This function is called whenever Re/Association Request need
 * to be rejected due to failure in assigning an AID or failure
 * in adding STA context at Polaris or reject by applications.
 *
 *LOGIC:
 * Resources allocated if any are freedup and (Re) Association
 * Response frame is sent to requesting STA. Pre-Auth context
 * will be added for this STA if it does not exist already
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  *pBd    - A pointer to Buffer descriptor + associated PDUs
 * @param  subType - Indicates whether it is Association Request (=0) or
 *                   Reassociation Request (=1) frame
 * @param  addPreAuthContext - Indicates whether pre-auth context
 *                             to be added for this STA
 * @param  authType - Indicates auth type to be added
 * @param  staId    - Indicates staId of the STA being rejected
 *                    association
 * @param  deleteSta - Indicates whether to delete STA context
 *                     at Polaris
 * @param  rCode    - Indicates what reasonCode to be sent in
 *                    Re/Assoc response to STA
 *
 * @return None
 */

void
limRejectAssociation(tpAniSirGlobal pMac, tSirMacAddr peerAddr, tANI_U8 subType,
                     tANI_U8 addPreAuthContext, tAniAuthType authType,
                     tANI_U16 staId, tANI_U8 deleteSta, tSirResultCodes rCode, tpPESession psessionEntry )
{
    tpDphHashNode       pStaDs;

    if (addPreAuthContext)
    {
        // Create entry for this STA in pre-auth list
        struct tLimPreAuthNode *pAuthNode;

        pAuthNode = limAcquireFreePreAuthNode(pMac, &pMac->lim.gLimPreAuthTimerTable);

        if (pAuthNode)
        {
            palCopyMemory( pMac->hHdd, (tANI_U8 *) pAuthNode->peerMacAddr,
                          peerAddr,
                          sizeof(tSirMacAddr));
            pAuthNode->fTimerStarted = 0;
            pAuthNode->mlmState = eLIM_MLM_AUTHENTICATED_STATE;
            pAuthNode->authType = (tAniAuthType) authType;
            limAddPreAuthNode(pMac, pAuthNode);
        }
    }

    if (deleteSta == true)
    {
        pStaDs = dphGetHashEntry(pMac, staId, &psessionEntry->dph.dphHashTable);

        if (pStaDs == NULL)
        {
            limLog(pMac, LOGW,
                   FL("No STA context, yet rejecting Association\n"));

            return;
        }

        /**
         * Polaris has state for this STA.
         * Trigger cleanup.
         */
        pStaDs->mlmStaContext.cleanupTrigger = eLIM_REASSOC_REJECT;

        // Receive path cleanup
        limCleanupRxPath(pMac, pStaDs, psessionEntry);
   
        // Send Re/Association Response with
        // status code to requesting STA.
        limSendAssocRspMgmtFrame(pMac,
                                 rCode,
                                 0,
                                 peerAddr,
                                 subType, 0,psessionEntry);

        if ( psessionEntry->parsedAssocReq[pStaDs->assocId] != NULL)
        {
            // Assoction confirmation is complete, free the copy of association request frame
            if ( ((tpSirAssocReq)(psessionEntry->parsedAssocReq[pStaDs->assocId]))->assocReqFrame) 
            {
                palFreeMemory(pMac->hHdd,((tpSirAssocReq)(psessionEntry->parsedAssocReq[pStaDs->assocId]))->assocReqFrame);
                ((tpSirAssocReq)(psessionEntry->parsedAssocReq[pStaDs->assocId]))->assocReqFrame = NULL;
            }            
            palFreeMemory(pMac->hHdd, psessionEntry->parsedAssocReq[pStaDs->assocId]);    
            psessionEntry->parsedAssocReq[pStaDs->assocId] = NULL;
        }
    }
    else
    {
        limSendAssocRspMgmtFrame(pMac,
                                 eSIR_MAC_MAX_ASSOC_STA_REACHED_STATUS,
                                 1,
                                 peerAddr,
                                 subType, 0,psessionEntry);
        // Log error
        limLog(pMac, LOGW,
           FL("received Re/Assoc req when max associated STAs reached from \n"));
        limPrintMacAddr(pMac, peerAddr, LOGW);
        limSendSmeMaxAssocExceededNtf(pMac, peerAddr, psessionEntry->smeSessionId);
    }
} /*** end limRejectAssociation() ***/

#ifdef WLAN_SOFTAP_FEATURE

/** -------------------------------------------------------------
\fn limDecideApProtectionOnHt20Delete
\brief protection related function while HT20 station is getting deleted.
\param      tpAniSirGlobal    pMac
\param      tpDphHashNode pStaDs
\param      tpUpdateBeaconParams pBeaconParams
\return      None
  -------------------------------------------------------------*/
static void
limDecideApProtectionOnHt20Delete(tpAniSirGlobal pMac, 
      tpDphHashNode pStaDs, tpUpdateBeaconParams pBeaconParams,tpPESession psessionEntry)
{
    tANI_U32 i = 0;
   PELOG1( limLog(pMac, LOG1, FL("(%d) A HT 20 STA is disassociated. Addr is "),
           psessionEntry->gLimHt20Params.numSta);
    limPrintMacAddr(pMac, pStaDs->staAddr, LOG1);)
    if (psessionEntry->gLimHt20Params.numSta > 0)
    {
        for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
        {
            if (psessionEntry->protStaCache[i].active)
            {
                if (palEqualMemory( pMac->hHdd,psessionEntry->protStaCache[i].addr,
                        pStaDs->staAddr, sizeof(tSirMacAddr)))
                {
                    psessionEntry->gLimHt20Params.numSta--;
                    psessionEntry->protStaCache[i].active = false;
                    break;
                }
            }
        }
    }

    if (psessionEntry->gLimHt20Params.numSta == 0)
    {
        // disable protection
        limEnableHT20Protection(pMac, false, false, pBeaconParams,psessionEntry);
    }
}
/** -------------------------------------------------------------
\fn limDecideApProtectionOnDelete
\brief Decides about protection related settings when a station is getting deleted.
\param      tpAniSirGlobal    pMac
\param      tpDphHashNode pStaDs
\param      tpUpdateBeaconParams pBeaconParams
\return      None
  -------------------------------------------------------------*/
void
limDecideApProtectionOnDelete(tpAniSirGlobal pMac, 
      tpDphHashNode pStaDs, tpUpdateBeaconParams pBeaconParams,tpPESession psessionEntry)
{
    tANI_U32 phyMode;
    tHalBitVal erpEnabled = eHAL_CLEAR;
    tSirRFBand rfBand = SIR_BAND_UNKNOWN;
    tANI_U32 i;    
    
    if(NULL == pStaDs)
      return;

    limGetRfBand(pMac, &rfBand, psessionEntry);
    if(SIR_BAND_5_GHZ == rfBand)
    {
        //we are HT. if we are 11A, then protection is not required.
        if(true == psessionEntry->htCapability)
        {
            //we are HT and 11A station is leaving.
            //protection consideration required.
            //HT station leaving ==> this case is commonly handled between both the bands below.
            if((psessionEntry->beaconParams.llaCoexist) && 
              (false == pStaDs->mlmStaContext.htCapability))
            {
                PELOG1(limLog(pMac, LOG1, FL("(%d) A 11A STA is disassociated. Addr is "),
                       psessionEntry->gLim11aParams.numSta);
                limPrintMacAddr(pMac, pStaDs->staAddr, LOG1);)
                if (psessionEntry->gLim11aParams.numSta > 0)
                {
                    for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
                    {
                        if (psessionEntry->protStaCache[i].active)
                        {
                            if (palEqualMemory( pMac->hHdd,psessionEntry->protStaCache[i].addr,
                                    pStaDs->staAddr, sizeof(tSirMacAddr)))
                            {
                                psessionEntry->gLim11aParams.numSta--;
                                psessionEntry->protStaCache[i].active = false;
                                break;
                            }
                        }
                    }
                }

                if(psessionEntry->gLim11aParams.numSta == 0) 
                {
                    // disable protection
                      limEnable11aProtection(pMac, false, false, pBeaconParams,psessionEntry);
                }
            }
        }
    }
    else if(SIR_BAND_2_4_GHZ == rfBand)
    {
        limGetPhyMode(pMac, &phyMode, psessionEntry);

        erpEnabled = pStaDs->erpEnabled;
        //we are HT or 11G and 11B station is getting deleted.
        if (((phyMode == WNI_CFG_PHY_MODE_11G) ||
              psessionEntry->htCapability) &&
              (erpEnabled == eHAL_CLEAR))
        {
            PELOG1(limLog(pMac, LOG1, FL("(%d) A legacy STA is disassociated. Addr is "),
                   psessionEntry->gLim11bParams.numSta);
            limPrintMacAddr(pMac, pStaDs->staAddr, LOG1);)
            if (psessionEntry->gLim11bParams.numSta > 0)
            {
                for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
                {
                    if (psessionEntry->protStaCache[i].active)
                    {
                        if (palEqualMemory( pMac->hHdd,psessionEntry->protStaCache[i].addr,
                                pStaDs->staAddr, sizeof(tSirMacAddr)))
                        {
                            psessionEntry->gLim11bParams.numSta--;
                            psessionEntry->protStaCache[i].active = false;
                            break;
                        }
                    }
                }
            }

            if (psessionEntry->gLim11bParams.numSta == 0)
            {
                // disable protection
                PELOG1(limLog(pMac, LOG1, FL("No 11B STA exists\n"));)
                limEnable11gProtection(pMac, false, false, pBeaconParams,psessionEntry);
            }
        }
        //(non-11B station is leaving) or (we are not 11G or HT AP)
        else if(psessionEntry->htCapability)
        { //we are HT AP and non-11B station is leaving.

            //11g station is leaving            
            if(!pStaDs->mlmStaContext.htCapability)
            {
                PELOG1(limLog(pMac, LOG1, FL("(%d) A 11g STA is disassociated. Addr is "),
                       psessionEntry->gLim11bParams.numSta);
                limPrintMacAddr(pMac, pStaDs->staAddr, LOG1);)
                if (psessionEntry->gLim11gParams.numSta > 0)
                {
                    for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
                    {
                        if (psessionEntry->protStaCache[i].active)
                        {
                            if (palEqualMemory( pMac->hHdd,psessionEntry->protStaCache[i].addr,
                                    pStaDs->staAddr, sizeof(tSirMacAddr)))
                            {
                                psessionEntry->gLim11gParams.numSta--;
                                psessionEntry->protStaCache[i].active = false;
                                break;
                            }
                        }
                    }
                }

                if (psessionEntry->gLim11gParams.numSta == 0) 
                {
                    // disable protection
                      limEnableHtProtectionFrom11g(pMac, false, false, pBeaconParams,psessionEntry);
                  }
            }              
        }
    }

    //LSIG TXOP not supporting staiton leaving. applies to 2.4 as well as 5 GHZ.
    if((true == psessionEntry->htCapability) &&
        (true == pStaDs->mlmStaContext.htCapability))
    {
        //HT non-GF leaving
        if(!pStaDs->htGreenfield)
        {
            PELOG1(limLog(pMac, LOG1, FL("(%d) A non-GF STA is disassociated. Addr is "),
                   psessionEntry->gLimNonGfParams.numSta);
            limPrintMacAddr(pMac, pStaDs->staAddr, LOG1);)
            if (psessionEntry->gLimNonGfParams.numSta > 0)
            {
                for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
                {
                    if (psessionEntry->protStaCache[i].active)
                    {
                        if (palEqualMemory( pMac->hHdd,psessionEntry->protStaCache[i].addr,
                                pStaDs->staAddr, sizeof(tSirMacAddr)))
                        {
                            psessionEntry->gLimNonGfParams.numSta--;
                            psessionEntry->protStaCache[i].active = false;
                            break;
                        }
                    }
                }
            }

            if (psessionEntry->gLimNonGfParams.numSta == 0)
            {
                // disable protection
                limEnableHTNonGfProtection(pMac, false, false, pBeaconParams,psessionEntry);
            }
        }
        //HT 20Mhz station leaving.
        if(psessionEntry->beaconParams.ht20Coexist &&
                (eHT_CHANNEL_WIDTH_20MHZ == pStaDs->htSupportedChannelWidthSet))
        {
            limDecideApProtectionOnHt20Delete(pMac, pStaDs, pBeaconParams,psessionEntry);
        }

        if(false == psessionEntry->beaconParams.fLsigTXOPProtectionFullSupport &&
                (false == pStaDs->htLsigTXOPProtection))
        {
           PELOG1( limLog(pMac, LOG1, FL("(%d) A HT LSIG not supporting STA is disassociated. Addr is "),
                   psessionEntry->gLimLsigTxopParams.numSta);
            limPrintMacAddr(pMac, pStaDs->staAddr, LOG1);)
            if (psessionEntry->gLimLsigTxopParams.numSta > 0)
            {
                for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
                {
                    if (psessionEntry->protStaCache[i].active)
                    {
                        if (palEqualMemory( pMac->hHdd,psessionEntry->protStaCache[i].addr,
                                pStaDs->staAddr, sizeof(tSirMacAddr)))
                        {
                            psessionEntry->gLimLsigTxopParams.numSta--;
                            psessionEntry->protStaCache[i].active = false;
                            break;
                        }
                    }
                }
            }

            if (psessionEntry->gLimLsigTxopParams.numSta == 0)
            {
                // disable protection
                limEnableHTLsigTxopProtection(pMac, true, false, pBeaconParams,psessionEntry);
            }
        }    
    }
}

#endif


/** -------------------------------------------------------------
\fn limDecideShortPreamble
\brief Decides about any short preamble reated change because of new station joining.
\param      tpAniSirGlobal    pMac
\param      tpDphHashNode pStaDs
\param      tpUpdateBeaconParams pBeaconParams
\return      None
  -------------------------------------------------------------*/
void limDecideShortPreamble(tpAniSirGlobal pMac, 
          tpDphHashNode pStaDs, tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry )
{
   tANI_U32 i;

   if (pStaDs->shortPreambleEnabled == eHAL_CLEAR)
   {
      PELOG1(limLog(pMac, LOG1, FL("(%d) A non-short preamble STA is disassociated. Addr is "),
             psessionEntry->gLimNoShortParams.numNonShortPreambleSta);
      limPrintMacAddr(pMac, pStaDs->staAddr, LOG1);)
      if (psessionEntry->gLimNoShortParams.numNonShortPreambleSta > 0)
      {
          for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
          {
             if (psessionEntry->gLimNoShortParams.staNoShortCache[i].active)
             {
                if (palEqualMemory( pMac->hHdd, psessionEntry->gLimNoShortParams.staNoShortCache[i].addr,
                                   pStaDs->staAddr, sizeof(tSirMacAddr)))
                {
                    psessionEntry->gLimNoShortParams.numNonShortPreambleSta--;
                    psessionEntry->gLimNoShortParams.staNoShortCache[i].active = false;
                    break;
                }
             }
          }
      }

      if (psessionEntry->gLimNoShortParams.numNonShortPreambleSta == 0)
      {
         // enable short preamble
         PELOG1(limLog(pMac, LOG1, FL("All associated STAs have short preamble support now.\n"));)
         //reset the cache
         palZeroMemory( pMac->hHdd, (tANI_U8 *)&psessionEntry->gLimNoShortParams , sizeof(tLimNoShortParams));
         if (limEnableShortPreamble(pMac, true, pBeaconParams, psessionEntry) != eSIR_SUCCESS)
             PELOGE(limLog(pMac, LOGE, FL("Cannot enable short preamble\n"));)
      }
   }
}

/** -------------------------------------------------------------
\fn limDecideShortSlot
\brief Decides about any short slot time related change because of station leaving the BSS.
\param      tpAniSirGlobal    pMac
\param      tpDphHashNode pStaDs
\return      None
  -------------------------------------------------------------*/

void
limDecideShortSlot(tpAniSirGlobal pMac, tpDphHashNode pStaDs,  
                   tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry)
{
  tANI_U32 i, val;
  if (pStaDs->shortSlotTimeEnabled == eHAL_CLEAR)
  {
    PELOG1(limLog(pMac, LOG1, FL("(%d) A non-short slottime STA is disassociated. Addr is "),
       pMac->lim.gLimNoShortSlotParams.numNonShortSlotSta);
       limPrintMacAddr(pMac, pStaDs->staAddr, LOG1);)

#ifdef WLAN_SOFTAP_FEATURE
    if ((psessionEntry->limSystemRole == eLIM_AP_ROLE ) &&
       psessionEntry->gLimNoShortSlotParams.numNonShortSlotSta> 0)
       {
          for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
          {
             if (psessionEntry->gLimNoShortSlotParams.staNoShortSlotCache[i].active)
             {
                if (palEqualMemory( pMac->hHdd, psessionEntry->gLimNoShortSlotParams.staNoShortSlotCache[i].addr,
                    pStaDs->staAddr, sizeof(tSirMacAddr)))
                    {
                        psessionEntry->gLimNoShortSlotParams.numNonShortSlotSta--;
                        psessionEntry->gLimNoShortSlotParams.staNoShortSlotCache[i].active = false;
                        break;
                    }
             }
          }
       }
       else
#endif
       {
           if (pMac->lim.gLimNoShortSlotParams.numNonShortSlotSta> 0)
           {
             for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
             {
               if (pMac->lim.gLimNoShortSlotParams.staNoShortSlotCache[i].active)
               {
                  if (palEqualMemory( pMac->hHdd, pMac->lim.gLimNoShortSlotParams.staNoShortSlotCache[i].addr,
                      pStaDs->staAddr, sizeof(tSirMacAddr)))
                  {
                      pMac->lim.gLimNoShortSlotParams.numNonShortSlotSta--;
                      pMac->lim.gLimNoShortSlotParams.staNoShortSlotCache[i].active = false;
                      break;
                  }
               }
             }
           }
         }

      wlan_cfgGetInt(pMac, WNI_CFG_11G_SHORT_SLOT_TIME_ENABLED, &val);

#ifdef WLAN_SOFTAP_FEATURE
      if ( (psessionEntry->limSystemRole == eLIM_AP_ROLE ) &&
         (val && psessionEntry->gLimNoShortSlotParams.numNonShortSlotSta == 0))
      {
         // enable short slot time
         PELOG1(limLog(pMac, LOG1, FL("All associated STAs have short slot time support now.\n"));)
         //reset the cache
         palZeroMemory( pMac->hHdd, (tANI_U8 *)&psessionEntry->gLimNoShortSlotParams , sizeof(tLimNoShortSlotParams));
         // in case of AP set SHORT_SLOT_TIME to enable
         if (psessionEntry->limSystemRole == eLIM_AP_ROLE)
         {
            pBeaconParams->fShortSlotTime = true;
            pBeaconParams->paramChangeBitmap |= PARAM_SHORT_SLOT_TIME_CHANGED;
            psessionEntry->shortSlotTimeSupported = true;
         }
      }
      else 
#endif
      {
         if (val && pMac->lim.gLimNoShortSlotParams.numNonShortSlotSta == 0)
         {
            // enable short slot time
            PELOG1(limLog(pMac, LOG1, FL("All associated STAs have short slot time support now.\n"));)
            //reset the cache
            palZeroMemory( pMac->hHdd, (tANI_U8 *)&pMac->lim.gLimNoShortSlotParams , sizeof(tLimNoShortSlotParams));
            // in case of AP set SHORT_SLOT_TIME to enable
            if (psessionEntry->limSystemRole == eLIM_AP_ROLE)
            {
               pBeaconParams->fShortSlotTime = true;
               pBeaconParams->paramChangeBitmap |= PARAM_SHORT_SLOT_TIME_CHANGED;
               psessionEntry->shortSlotTimeSupported = true;
            }
          }
       }
   }
}

void
limPostReassocFailure(tpAniSirGlobal pMac,
                      tSirResultCodes resultCode,
                      tANI_U16 protStatusCode,tpPESession psessionEntry)
{
    tLimMlmReassocCnf   mlmReassocCnf;

    psessionEntry->limMlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, eLIM_MLM_LINK_ESTABLISHED_STATE));

    // 'Change' timer for future activations
    limDeactivateAndChangeTimer(pMac, eLIM_REASSOC_FAIL_TIMER);

    mlmReassocCnf.resultCode = resultCode;
    mlmReassocCnf.protStatusCode = protStatusCode;
    /* Update PE session Id */
    mlmReassocCnf.sessionId = psessionEntry->peSessionId;
    limPostSmeMessage(pMac,
                      LIM_MLM_REASSOC_CNF,
                      (tANI_U32 *) &mlmReassocCnf);
} /*** end limPostReassocFailure() ***/

/**
 * limRestorePreReassocState()
 *
 *FUNCTION:
 * This function is called on STA role whenever Reasociation
 * Response with a reject code is received from AP.
 *
 *LOGIC:
 * Reassociation failure timer is stopped, Old (or current) AP's
 * context is restored both at Polaris & software
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac       - Pointer to Global MAC structure
 * @param  resultCode - Result code that specifies why Reassociation
 *                      attemp failed
 *
 * @return None
 */

void
limRestorePreReassocState(tpAniSirGlobal pMac,
                          tSirResultCodes resultCode,
                          tANI_U16 protStatusCode,tpPESession psessionEntry)
{
    tANI_U8             chanNum, secChanOffset;
    tLimMlmReassocCnf   mlmReassocCnf;

    psessionEntry->limMlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, eLIM_MLM_LINK_ESTABLISHED_STATE));

    // 'Change' timer for future activations
    limDeactivateAndChangeTimer(pMac, eLIM_REASSOC_FAIL_TIMER);

    // Update BSSID at CFG database
    #if 0
    if (cfgSetStr(pMac, WNI_CFG_BSSID,
                  pMac->lim.gLimCurrentBssId,
                  sizeof(tSirMacAddr)) != eSIR_SUCCESS)
    {
        /// Could not update BSSID at CFG. Log error.
        limLog(pMac, LOGP, FL("could not update BSSID at CFG\n"));
        return;
    }
    #endif

   // chanNum = pMac->lim.gLimCurrentChannelId;

   /*  To support BT-AMP */
   chanNum = psessionEntry->currentOperChannel;
   secChanOffset = psessionEntry->htSecondaryChannelOffset;

    limSetChannel(pMac, chanNum, secChanOffset, psessionEntry->maxTxPower, psessionEntry->peSessionId);

    /** @ToDo : Need to Integrate the STOP the DataTransfer to the AP from 11H code */

    mlmReassocCnf.resultCode = resultCode;
    mlmReassocCnf.protStatusCode = protStatusCode;
    /* Update PE session Id */
    mlmReassocCnf.sessionId = psessionEntry->peSessionId;
    limPostSmeMessage(pMac,
                      LIM_MLM_REASSOC_CNF,
                      (tANI_U32 *) &mlmReassocCnf);
} /*** end limRestorePreReassocState() ***/



/**
 * limIsReassocInProgress()
 *
 *FUNCTION:
 * This function is called to see if STA is in wt-reassoc-rsp state.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac    - Pointer to Global MAC structure
 *
 * @return eANI_BOOLEAN_TRUE  When STA is waiting for Reassoc response from AP \n
 *         else eANI_BOOLEAN_FALSE
 */

eAniBoolean
limIsReassocInProgress(tpAniSirGlobal pMac,tpPESession psessionEntry)
{
    if(((psessionEntry->limSystemRole == eLIM_STA_ROLE) || (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE))&&
            ((psessionEntry->limSmeState == eLIM_SME_WT_REASSOC_STATE) || 
               (psessionEntry->limSmeState == eLIM_SME_WT_REASSOC_LINK_FAIL_STATE)))
        return eANI_BOOLEAN_TRUE;
        
    return eANI_BOOLEAN_FALSE;
} /*** end limIsReassocInProgress() ***/

#ifdef WLAN_FEATURE_11AC
tSirRetStatus limPopulateVhtMcsSet(tpAniSirGlobal pMac,
                                  tpSirSupportedRates pRates,
                                  tDot11fIEVHTCaps *pPeerVHTCaps,
                                  tpPESession psessionEntry)
{
    tANI_U32 val;

    if(IS_DOT11_MODE_VHT(psessionEntry->dot11mode))
    {
        if ( wlan_cfgGetInt( pMac,WNI_CFG_VHT_RX_MCS_MAP,&val) != 
            eSIR_SUCCESS )
        {
            PELOGE(limLog(pMac, LOGE, FL("could not retrieve VHT RX MCS MAP\n"));)
            goto error;
        }
        pRates->vhtRxMCSMap = (tANI_U16)val;
    
        if ( wlan_cfgGetInt( pMac,WNI_CFG_VHT_TX_MCS_MAP,&val ) != 
            eSIR_SUCCESS )
        {
            PELOGE(limLog(pMac, LOGE, FL("could not retrieve VHT TX MCS MAP\n"));)
            goto error;
        }
        pRates->vhtTxMCSMap = (tANI_U16)val;

        if ( wlan_cfgGetInt( pMac,WNI_CFG_VHT_RX_HIGHEST_SUPPORTED_DATA_RATE,&val ) != 
            eSIR_SUCCESS )
        {
            PELOGE(limLog(pMac, LOGE, FL("could not retrieve VHT RX Supported data rate MAP\n"));)
            goto error;
        }
        pRates->vhtRxHighestDataRate = (tANI_U16)val;
    
        if ( wlan_cfgGetInt( pMac,WNI_CFG_VHT_TX_HIGHEST_SUPPORTED_DATA_RATE,&val ) != 
            eSIR_SUCCESS )
        {
            PELOGE(limLog(pMac, LOGE, FL("could not retrieve VHT RX Supported data rate MAP\n"));)
            goto error;
        }
        pRates->vhtTxHighestDataRate = (tANI_U16)val;

        if( pPeerVHTCaps != NULL)
        {
            pRates->vhtTxHighestDataRate = SIR_MIN(pRates->vhtTxHighestDataRate, pPeerVHTCaps->txSupDataRate);
            pRates->vhtRxHighestDataRate = SIR_MIN(pRates->vhtRxHighestDataRate, pPeerVHTCaps->rxHighSupDataRate);

            // Acquire PEER MCS map if we exceed.
            // We compare/update only the last 2 bits of the map as we support only single BSS.
            // Firmware takes care of this comparison
            pRates->vhtRxMCSMap &= ~(0x3); // Clearing the last 2 bits in the bitmap
            pRates->vhtRxMCSMap |= (pPeerVHTCaps->rxMCSMap & 0x3); // Updating the last 2 bits in the bitmap

            // Firmware takes care of this comparison
            pRates->vhtTxMCSMap &= ~(0x3); // Clearing the last 2 bits in the bitmap
            pRates->vhtTxMCSMap |= (pPeerVHTCaps->txMCSMap & 0x3); // Updating the last 2 bits in the bitmap
        }
    }
    return eSIR_SUCCESS;
error:

    return eSIR_FAILURE;

}
#endif

/**
 * limPopulateOwnRateSet
 *
 * FUNCTION:
 * This function is called by limProcessAssocRsp() or
 * limAddStaInIBSS()
 * - It creates a combined rate set of 12 rates max which
 *   comprises the basic and extended rates read from CFG
 * - It sorts the combined rate Set and copy it in the
 *   rate array of the pSTA descriptor
 * - It sets the erpEnabled bit of the STA descriptor
 *
 * NOTE:
 * ERP bit is set iff the dph PHY mode is 11G and there is at least
 * an A rate in the supported or extended rate sets
 *
 * @param  pMac      - Pointer to Global MAC structure
 * @param  basicOnly - When passed value is true, only basic
 *                     rates are copied to DPH node else
 *                     all supported rates are copied
 * @return eSIR_SUCCESS or eSIR_FAILURE
 *
 */
#ifdef WLAN_FEATURE_11AC
tSirRetStatus
limPopulateOwnRateSet(tpAniSirGlobal pMac,
                      tpSirSupportedRates pRates,
                      tANI_U8* pSupportedMCSSet,
                      tANI_U8 basicOnly,
                      tpPESession psessionEntry,
                      tDot11fIEVHTCaps *pVHTCaps)
#else
tSirRetStatus
limPopulateOwnRateSet(tpAniSirGlobal pMac,
                      tpSirSupportedRates pRates,
                      tANI_U8* pSupportedMCSSet,
                      tANI_U8 basicOnly,
                      tpPESession psessionEntry)
#endif

{
    tSirMacRateSet          tempRateSet;
    tSirMacRateSet          tempRateSet2;
    tANI_U32                     i,j,val,min,isArate;
    tANI_U32 phyMode = 0;

    isArate = 0;

    limGetPhyMode(pMac, &phyMode, psessionEntry);

    // Get own rate set
    #if 0
    val = WNI_CFG_OPERATIONAL_RATE_SET_LEN;
    if (wlan_cfgGetStr(pMac, WNI_CFG_OPERATIONAL_RATE_SET,
                  (tANI_U8 *) &tempRateSet.rate,
                  &val) != eSIR_SUCCESS)
    {
        /// Could not get rateset from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve rateset\n"));
    }
    #endif // TO SUPPORT BT-AMP
    
    /* copy operational rate set from psessionEntry */
    palCopyMemory(pMac->hHdd,(tANI_U8 *)tempRateSet.rate,(tANI_U8*)(psessionEntry->rateSet.rate), psessionEntry->rateSet.numRates);
    tempRateSet.numRates = psessionEntry->rateSet.numRates;

    if (phyMode == WNI_CFG_PHY_MODE_11G)
    {

        // get own extended rate set
        #if 0
        val = WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET_LEN;
        if (wlan_cfgGetStr(pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
                      (tANI_U8 *) &tempRateSet2.rate,
                      &val) != eSIR_SUCCESS)
        {
            /// Could not get extended rateset from CFG. Log error.
            limLog(pMac, LOGP, FL("could not retrieve extended rateset\n"));
        }
        tempRateSet2.numRates = (tANI_U8) val;
        #endif

        palCopyMemory(pMac->hHdd,(tANI_U8 *)tempRateSet2.rate, (tANI_U8*)(psessionEntry->extRateSet.rate), psessionEntry->extRateSet.numRates);
        tempRateSet2.numRates = psessionEntry->extRateSet.numRates;

    }
    else
        tempRateSet2.numRates = 0;


    if ((tempRateSet.numRates + tempRateSet2.numRates) > 12)
    {
        //we are in big trouble
        limLog(pMac, LOGP, FL("more than 12 rates in CFG\n"));
        //panic
        goto error;
    }


    //copy all rates in tempRateSet, there are 12 rates max
    for (i = 0;i < tempRateSet2.numRates; i++)
      tempRateSet.rate[i + tempRateSet.numRates] = tempRateSet2.rate[i];
    tempRateSet.numRates += tempRateSet2.numRates;

    /**
     * Sort rates in tempRateSet (they are likely to be already sorted)
     * put the result in pSupportedRates
     */
    {
        tANI_U8 aRateIndex = 0;
        tANI_U8 bRateIndex = 0;

        palZeroMemory( pMac->hHdd, (tANI_U8 *) pRates, sizeof(tSirSupportedRates));
        for(i = 0;i < tempRateSet.numRates; i++)
        {
            min = 0;
            val = 0xff;
            isArate = 0;
            for(j = 0;j < tempRateSet.numRates; j++)
            {
                if ((tANI_U32) (tempRateSet.rate[j] & 0x7f) < val)
                {
                     val = tempRateSet.rate[j] & 0x7f;
                     min = j;
                }
            }

            if (sirIsArate(tempRateSet.rate[min] & 0x7f))
                isArate = 1;

    /*
    * HAL needs to know whether the rate is basic rate or not, as it needs to 
    * update the response rate table accordingly. e.g. if one of the 11a rates is
    * basic rate, then that rate can be used for sending control frames.
    * HAL updates the response rate table whenever basic rate set is changed.
    */
            if (basicOnly)
            {
                if (tempRateSet.rate[min] & 0x80)
                {
                    if (isArate)
                        pRates->llaRates[aRateIndex++] = tempRateSet.rate[min];
                    else
                        pRates->llbRates[bRateIndex++] = tempRateSet.rate[min];
                }
            }
            else
            {
                if (isArate)
                    pRates->llaRates[aRateIndex++] = tempRateSet.rate[min];
                else
                    pRates->llbRates[bRateIndex++] = tempRateSet.rate[min];
            }
            tempRateSet.rate[min] = 0xff;
        }

    }


    if(IS_DOT11_MODE_HT(psessionEntry->dot11mode))
    {
        val = SIZE_OF_SUPPORTED_MCS_SET;
        if (wlan_cfgGetStr(pMac, WNI_CFG_SUPPORTED_MCS_SET,
                      pRates->supportedMCSSet,
                      &val) != eSIR_SUCCESS)
        {
            /// Could not get rateset from CFG. Log error.
            PELOGE(limLog(pMac, LOGE, FL("could not retrieve supportedMCSSet\n"));)
            goto error;
        }


        //if supported MCS Set of the peer is passed in, then do the intersection
        //else use the MCS set from local CFG.

        if(pSupportedMCSSet != NULL)
        {
            for(i=0; i<SIR_MAC_MAX_SUPPORTED_MCS_SET; i++)
                    pRates->supportedMCSSet[i] &= pSupportedMCSSet[i];

        }

        PELOG2(limLog(pMac, LOG2, FL("MCS Rate Set Bitmap: \n"));)
        for(i=0; i<SIR_MAC_MAX_SUPPORTED_MCS_SET; i++)
            PELOGW(limLog(pMac, LOG2,FL("%x ") , pRates->supportedMCSSet[i]);)
    }

#ifdef WLAN_FEATURE_11AC
    limPopulateVhtMcsSet(pMac, pRates , pVHTCaps,psessionEntry);
#endif

    return eSIR_SUCCESS;

 error:

    return eSIR_FAILURE;
} /*** limPopulateOwnRateSet() ***/

/**
 * limPopulateMatchingRateSet
 * FUNCTION:
 * This is called at the time of Association Request
 * processing on AP and while adding peer's context
 * in IBSS role to process the CFG rate sets and
 * the rate sets received in the Assoc request on AP
 * or Beacon/Probe Response from peer in IBSS.
 *
 * LOGIC:
 * 1. It makes the intersection between our own rate Sat
 *    and extemcded rate set and the ones received in the
 *    association request.
 * 2. It creates a combined rate set of 12 rates max which
 *    comprised the basic and extended rates
 * 3. It sorts the combined rate Set and copy it in the
 *    rate array of the pSTA descriptor
 *
 * ASSUMPTION:
 * The parser has already ensured unicity of the rates in the
 * association request structure
 *
 * @param: pMac         - Pointer to Global MAC structure
 *         pStaDs       - Pointer to DPH node
 *         pOperRateSet - Pointer to peer's supported rateset
 *         pExtRateSet  - Pointer to peer's extended rateset
 *
 * @return:  eSIR_SUCCESS or eSIR_FAILURE
 */
#ifdef WLAN_FEATURE_11AC
tSirRetStatus
limPopulateMatchingRateSet(tpAniSirGlobal pMac,
                           tpDphHashNode pStaDs,
                           tSirMacRateSet *pOperRateSet,
                           tSirMacRateSet *pExtRateSet,
                           tANI_U8* pSupportedMCSSet,
                           tSirMacPropRateSet *pAniLegRateSet,
                           tpPESession  psessionEntry,
                           tDot11fIEVHTCaps *pVHTCaps)

#else
tSirRetStatus
limPopulateMatchingRateSet(tpAniSirGlobal pMac,
                           tpDphHashNode pStaDs,
                           tSirMacRateSet *pOperRateSet,
                           tSirMacRateSet *pExtRateSet,
                           tANI_U8* pSupportedMCSSet,
                           tSirMacPropRateSet *pAniLegRateSet,
                           tpPESession  psessionEntry)
#endif
{
   tSirMacRateSet          tempRateSet;
   tSirMacRateSet          tempRateSet2;
   tANI_U32                i,j,val,min,isArate;
   tANI_U32 phyMode;
   tANI_U8 mcsSet[SIZE_OF_SUPPORTED_MCS_SET];

   isArate=0;

   // limGetPhyMode(pMac, &phyMode);
   limGetPhyMode(pMac, &phyMode, psessionEntry);

   // get own rate set
   // val = WNI_CFG_OPERATIONAL_RATE_SET_LEN;
   #if 0
    if (wlan_cfgGetStr(pMac, WNI_CFG_OPERATIONAL_RATE_SET,
                  (tANI_U8 *) &tempRateSet.rate,
                  &val) != eSIR_SUCCESS)
    {
        /// Could not get rateset from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve rateset\n"));
    }

    #endif // TO SUPPORT BT-AMP

    /* copy operational rate set from psessionEntry */
    palCopyMemory(pMac->hHdd,(tempRateSet.rate),(psessionEntry->rateSet.rate),psessionEntry->rateSet.numRates);
    tempRateSet.numRates = (tANI_U8) psessionEntry->rateSet.numRates;

    if (phyMode == WNI_CFG_PHY_MODE_11G)
    {

        #if 0
        // get own extended rate set
        val = WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET_LEN;
        if (wlan_cfgGetStr(pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
                      (tANI_U8 *) &tempRateSet2.rate,
                      &val) != eSIR_SUCCESS)
        #endif
        palCopyMemory(pMac->hHdd,(tempRateSet2.rate),(psessionEntry->extRateSet.rate),psessionEntry->extRateSet.numRates);
        tempRateSet2.numRates = (tANI_U8) psessionEntry->extRateSet.numRates;
    }
    else
        tempRateSet2.numRates = 0;

    if ((tempRateSet.numRates + tempRateSet2.numRates) > 12)
    {
        PELOGE(limLog(pMac, LOGE, FL("more than 12 rates in CFG\n"));)
        goto error;
    }

    /**
     * Handling of the rate set IEs is the following:
     * - keep only rates that we support and that the station supports
     * - sort and the rates into the pSta->rate array
     */

    // Copy all rates in tempRateSet, there are 12 rates max
    for(i = 0; i < tempRateSet2.numRates; i++)
        tempRateSet.rate[i + tempRateSet.numRates] =
                                           tempRateSet2.rate[i];

    tempRateSet.numRates += tempRateSet2.numRates;

    /**
     * Sort rates in tempRateSet (they are likely to be already sorted)
     * put the result in tempRateSet2
     */
    tempRateSet2.numRates = 0;

    for(i = 0;i < tempRateSet.numRates; i++)
    {
        min = 0;
        val = 0xff;

        for(j = 0;j < tempRateSet.numRates; j++)
            if ((tANI_U32) (tempRateSet.rate[j] & 0x7f) < val)
            {
                val = tempRateSet.rate[j] & 0x7f;
                min = j;
            }

        tempRateSet2.rate[tempRateSet2.numRates++] =
                                              tempRateSet.rate[min];
        tempRateSet.rate[min] = 0xff;
    }


    /**
     * Copy received rates in tempRateSet, the parser has ensured
     * unicity of the rates so there cannot be more than 12
     */
    for(i = 0; i < pOperRateSet->numRates; i++)
    {
        tempRateSet.rate[i] = pOperRateSet->rate[i];
    }

    tempRateSet.numRates = pOperRateSet->numRates;

    if (pExtRateSet->numRates)
    {
      if((tempRateSet.numRates + pExtRateSet->numRates) > 12 )
      {
        limLog( pMac, LOG2,
            "Sum of SUPPORTED and EXTENDED Rate Set (%1d) exceeds 12!",
            tempRateSet.numRates + pExtRateSet->numRates );

        if( tempRateSet.numRates < 12 )
        {
         int found = 0;
         int tail = tempRateSet.numRates;

          for( i = 0; i < pExtRateSet->numRates; i++ )
          {
            found = 0;
            for( j = 0; j < (tANI_U32) tail; j++ )
            {
              if((tempRateSet.rate[j] & 0x7F) ==
                  (pExtRateSet->rate[i] & 0x7F))
              {
                found = 1;
                break;
              }
            }

            if( !found )
            {
              tempRateSet.rate[tempRateSet.numRates++] =
                pExtRateSet->rate[i];

              if( tempRateSet.numRates >= 12 )
                break;
            }
          }
        }
        else
          limLog( pMac, LOG2,
              "Relying only on the SUPPORTED Rate Set IE..." );
      }
      else
      {
        for(j = 0; j < pExtRateSet->numRates; j++)
            tempRateSet.rate[i+j] = pExtRateSet->rate[j];

        tempRateSet.numRates += pExtRateSet->numRates;
      }
    }

    {
        tpSirSupportedRates  rates = &pStaDs->supportedRates;
        tANI_U8 aRateIndex = 0;
        tANI_U8 bRateIndex = 0;
        palZeroMemory( pMac->hHdd, (tANI_U8 *) rates, sizeof(tSirSupportedRates));
        for(i = 0;i < tempRateSet2.numRates; i++)
        {
            for(j = 0;j < tempRateSet.numRates; j++)
            {
                if ((tempRateSet2.rate[i] & 0x7F) ==
                    (tempRateSet.rate[j] & 0x7F))
                { 
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
                    if ((bRateIndex > HAL_NUM_11B_RATES) || (aRateIndex > HAL_NUM_11A_RATES))
                    {
                        limLog(pMac, LOGE, FL("Invalid number of rates (11b->%d, 11a->%d)\n"),
                                            bRateIndex, aRateIndex);
                            return eSIR_FAILURE;
                    }
#endif
                    if (sirIsArate(tempRateSet2.rate[i] & 0x7f))
                    {
                        isArate=1;
                        rates->llaRates[aRateIndex++] = tempRateSet2.rate[i];
                    }
                    else
                        rates->llbRates[bRateIndex++] = tempRateSet2.rate[i];
                    break;
                }
            }
        }


        //Now add the Polaris rates only when Proprietary rates are enabled.
        val = 0;
        if(wlan_cfgGetInt(pMac, WNI_CFG_PROPRIETARY_RATES_ENABLED, &val) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP, FL("could not retrieve prop rate enabled flag from CFG\n"));
        }
        else if(val)
        {
            for(i=0; i<pAniLegRateSet->numPropRates; i++)
                rates->aniLegacyRates[i] = pAniLegRateSet->propRate[i];
        }

    }


    //compute the matching MCS rate set, if peer is 11n capable and self mode is 11n
#ifdef FEATURE_WLAN_TDLS
    if(pStaDs->mlmStaContext.htCapability)
#else
    if(IS_DOT11_MODE_HT(psessionEntry->dot11mode) &&
      (pStaDs->mlmStaContext.htCapability))
#endif
    {
        val = SIZE_OF_SUPPORTED_MCS_SET;
        if (wlan_cfgGetStr(pMac, WNI_CFG_SUPPORTED_MCS_SET,
                      mcsSet,
                      &val) != eSIR_SUCCESS)
        {
            /// Could not get rateset from CFG. Log error.
            limLog(pMac, LOGP, FL("could not retrieve supportedMCSSet\n"));
            goto error;
        }

        for(i=0; i<val; i++)
           pStaDs->supportedRates.supportedMCSSet[i] = mcsSet[i] & pSupportedMCSSet[i];

        PELOG2(limLog(pMac, LOG2, FL("limPopulateMatchingRateSet: MCS Rate Set Bitmap from  CFG and DPH : \n"));)
        for(i=0; i<SIR_MAC_MAX_SUPPORTED_MCS_SET; i++)
        {
            PELOG2(limLog(pMac, LOG2,FL("%x %x "), mcsSet[i], pStaDs->supportedRates.supportedMCSSet[i]);)
        }
    }

#ifdef WLAN_FEATURE_11AC
    limPopulateVhtMcsSet(pMac, &pStaDs->supportedRates, pVHTCaps, psessionEntry);
#endif
    /**
      * Set the erpEnabled bit iff the phy is in G mode and at least
      * one A rate is supported
      */
    if ((phyMode == WNI_CFG_PHY_MODE_11G) && isArate)
        pStaDs->erpEnabled = eHAL_SET;

    

    return eSIR_SUCCESS;

 error:

    return eSIR_FAILURE;
} /*** limPopulateMatchingRateSet() ***/



/**
 * limAddSta()
 *
 *FUNCTION:
 * This function is called to add an STA context at hardware
 * whenever a STA is (Re) Associated.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  pStaDs  - Pointer to the STA datastructure created by
 *                   LIM and maintained by DPH
 * @return retCode - Indicates success or failure return code
 */

tSirRetStatus
limAddSta(
    tpAniSirGlobal  pMac,
    tpDphHashNode   pStaDs,tpPESession psessionEntry)
{
    tpAddStaParams pAddStaParams = NULL;
    tSirMsgQ msgQ;
    tSirRetStatus     retCode = eSIR_SUCCESS;
    tSirMacAddr     staMac, *pStaAddr;
    tANI_U8 i;
#ifdef WLAN_FEATURE_P2P
    tpSirAssocReq   pAssocReq;
    tANI_U8  *p2pIe = NULL;
#endif
    #if 0
    retCode = wlan_cfgGetStr(pMac, WNI_CFG_STA_ID, staMac, &cfg);
    if (retCode != eSIR_SUCCESS)
            limLog(pMac, LOGP, FL("could not retrieve STA MAC\n"));
    #endif //To SUPPORT BT-AMP

    
    sirCopyMacAddr(staMac,psessionEntry->selfMacAddr);

    if( eHAL_STATUS_SUCCESS !=
        palAllocateMemory( pMac->hHdd, (void **) &pAddStaParams, sizeof( tAddStaParams )))
      {
        limLog( pMac, LOGP, FL( "Unable to allocate memory during ADD_STA\n" ));
        return eSIR_MEM_ALLOC_FAILED;
      }
    palZeroMemory( pMac->hHdd, (tANI_U8 *) pAddStaParams, sizeof(tAddStaParams));

    if ((limGetSystemRole(psessionEntry) == eLIM_AP_ROLE) ||
        (limGetSystemRole(psessionEntry) == eLIM_STA_IN_IBSS_ROLE) ||
        (limGetSystemRole(psessionEntry) == eLIM_BT_AMP_AP_ROLE) )
        pStaAddr = &pStaDs->staAddr;
#ifdef FEATURE_WLAN_TDLS
    /* SystemRole shouldn't be matter if staType is TDLS peer */
    else if(STA_ENTRY_TDLS_PEER == pStaDs->staType)
    {
        pStaAddr = &pStaDs->staAddr ;
    }
#endif
    else
        pStaAddr = &staMac;

    palCopyMemory( pMac->hHdd, (tANI_U8 *) pAddStaParams->staMac,
                  (tANI_U8 *) *pStaAddr, sizeof(tSirMacAddr));
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pAddStaParams->bssId,
                   psessionEntry->bssId, sizeof(tSirMacAddr));

    limFillSupportedRatesInfo(pMac, pStaDs, &pStaDs->supportedRates,psessionEntry);

    //Copy legacy rates
    palCopyMemory(pMac->hHdd, (tANI_U8*)&pAddStaParams->supportedRates,
                  (tANI_U8*)&pStaDs->supportedRates, sizeof(tSirSupportedRates));

    pAddStaParams->assocId = pStaDs->assocId;

    pAddStaParams->wmmEnabled = pStaDs->qosMode;
    pAddStaParams->listenInterval = pStaDs->mlmStaContext.listenInterval;
    pAddStaParams->shortPreambleSupported = pStaDs->shortPreambleEnabled;
#ifdef WLAN_SOFTAP_FEATURE
    if((limGetSystemRole(psessionEntry) == eLIM_AP_ROLE) && (pStaDs->mlmStaContext.subType == LIM_REASSOC))
    {
        /* TBD - need to remove this REASSOC check after fixinf rmmod issue */
        pAddStaParams->updateSta = pStaDs->mlmStaContext.updateContext;
    }
#endif
    pStaDs->valid                  = 0;
    pStaDs->mlmStaContext.mlmState = eLIM_MLM_WT_ADD_STA_RSP_STATE;

  // This will indicate HAL to "allocate" a new STA index
    pAddStaParams->staIdx = HAL_STA_INVALID_IDX;
    pAddStaParams->staType = pStaDs->staType;

    pAddStaParams->status = eHAL_STATUS_SUCCESS;
    pAddStaParams->respReqd = 1;
    //Update HT Capability

    if ((limGetSystemRole(psessionEntry) == eLIM_AP_ROLE) ||(limGetSystemRole(psessionEntry) == eLIM_BT_AMP_AP_ROLE) || (limGetSystemRole(psessionEntry) == eLIM_STA_IN_IBSS_ROLE))
    {
        pAddStaParams->htCapable = pStaDs->mlmStaContext.htCapability;
#ifdef WLAN_FEATURE_11AC
        pAddStaParams->vhtCapable = pStaDs->mlmStaContext.vhtCapability;
#endif
    }
#ifdef FEATURE_WLAN_TDLS
    /* SystemRole shouldn't be matter if staType is TDLS peer */
    else if(STA_ENTRY_TDLS_PEER == pStaDs->staType)
        pAddStaParams->htCapable = pStaDs->mlmStaContext.htCapability;
#endif
    else
    {
        pAddStaParams->htCapable = psessionEntry->htCapability;
#ifdef WLAN_FEATURE_11AC
        pAddStaParams->vhtCapable = psessionEntry->vhtCapability;
#endif

    }

    pAddStaParams->greenFieldCapable = pStaDs->htGreenfield;
    pAddStaParams->maxAmpduDensity= pStaDs->htAMpduDensity;
    pAddStaParams->maxAmpduSize = pStaDs->htMaxRxAMpduFactor;
    pAddStaParams->fDsssCckMode40Mhz = pStaDs->htDsssCckRate40MHzSupport;
    pAddStaParams->fShortGI20Mhz = pStaDs->htShortGI20Mhz;
    pAddStaParams->fShortGI40Mhz = pStaDs->htShortGI40Mhz;
    pAddStaParams->lsigTxopProtection = pStaDs->htLsigTXOPProtection;
    pAddStaParams->maxAmsduSize = pStaDs->htMaxAmsduLength;
    pAddStaParams->txChannelWidthSet = pStaDs->htSupportedChannelWidthSet;
    pAddStaParams->mimoPS = pStaDs->htMIMOPSState;

#ifdef WLAN_FEATURE_11AC
    if(pAddStaParams->vhtCapable)
    {
        pAddStaParams->vhtTxChannelWidthSet = pStaDs->vhtSupportedChannelWidthSet;
    }
#endif

    /* Update PE session ID*/
    pAddStaParams->sessionId = psessionEntry->peSessionId;

    if (psessionEntry->parsedAssocReq != NULL)
    {
#ifdef WLAN_FEATURE_P2P
    // Get a copy of the already parsed Assoc Request
       pAssocReq = (tpSirAssocReq) psessionEntry->parsedAssocReq[pStaDs->assocId];
       if ( pAssocReq && pAssocReq->addIEPresent && pAssocReq->addIE.length ) {
           p2pIe = limGetP2pIEPtr(pMac, pAssocReq->addIE.addIEdata, pAssocReq->addIE.length);
       }
       pAddStaParams->p2pCapableSta = (p2pIe != NULL);
#endif
    }

    //Disable BA. It will be set as part of ADDBA negotiation.
    for( i = 0; i < STACFG_MAX_TC; i++ )
    {
          pAddStaParams->staTCParams[i].txUseBA = eBA_DISABLE;
          pAddStaParams->staTCParams[i].rxUseBA = eBA_DISABLE;
    }

#ifdef WLAN_SOFTAP_FEATURE
#ifdef FEATURE_WLAN_TDLS
    if(pStaDs->wmeEnabled && \
       ((eLIM_AP_ROLE == psessionEntry->limSystemRole) || (STA_ENTRY_TDLS_PEER == pStaDs->staType)) )
#else
    if(pStaDs->wmeEnabled && (eLIM_AP_ROLE == psessionEntry->limSystemRole))
#endif      
    {
        pAddStaParams->uAPSD = 0;
        /* update UAPSD and send it to LIM to add STA */
        // bitmap MSB <- LSB MSB 4 bits are for
        // trigger enabled AC setting and LSB 4 bits
        // are for delivery enabled AC setting
        // 7   6    5    4    3    2    1    0
        // BE  BK   VI   VO   BE   BK   VI   VO
        pAddStaParams->uAPSD |= pStaDs->qos.capability.qosInfo.acvo_uapsd;
        pAddStaParams->uAPSD |= (pStaDs->qos.capability.qosInfo.acvi_uapsd << 1);
        pAddStaParams->uAPSD |= (pStaDs->qos.capability.qosInfo.acbk_uapsd << 2);
        pAddStaParams->uAPSD |= (pStaDs->qos.capability.qosInfo.acbe_uapsd << 3);
        //making delivery enabled and trigger enabled setting the same.
        pAddStaParams->uAPSD |= pAddStaParams->uAPSD << 4;

        pAddStaParams->maxSPLen = pStaDs->qos.capability.qosInfo.maxSpLen;
        limLog( pMac, LOG1, FL( "uAPSD = 0x%x, maxSpLen = %d" ),
            pAddStaParams->uAPSD, pAddStaParams->maxSPLen);
    }
#endif
  //we need to defer the message until we get the response back from HAL.
    if (pAddStaParams->respReqd)
        SET_LIM_PROCESS_DEFD_MESGS(pMac, false);

    msgQ.type = WDA_ADD_STA_REQ;

    msgQ.reserved = 0;
    msgQ.bodyptr = pAddStaParams;
    msgQ.bodyval = 0;

    limLog( pMac, LOG1, FL( "Sending SIR_HAL_ADD_STA_REQ for assocId %d\n" ),
            pStaDs->assocId);
    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));

    retCode = wdaPostCtrlMsg( pMac, &msgQ );
    if( eSIR_SUCCESS != retCode)
    {
       if (pAddStaParams->respReqd)
          SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
        limLog( pMac, LOGE, FL("ADD_STA_REQ for aId %d failed (reason %X)\n"),
                            pStaDs->assocId, retCode );
        palFreeMemory(pMac->hHdd, (void*)pAddStaParams);
    }

  return retCode;
}


/**
 * limDelSta()
 *
 *FUNCTION:
 * This function is called to delete an STA context at hardware
 * whenever a STA is disassociated
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  pStaDs  - Pointer to the STA datastructure created by
 *                   LIM and maintained by DPH
 * @param  fRespReqd - flag to indicate whether the delete is synchronous (true)
 *                   or not (false)
 * @return retCode - Indicates success or failure return code
 */

tSirRetStatus
limDelSta(
    tpAniSirGlobal  pMac,
    tpDphHashNode   pStaDs,
    tANI_BOOLEAN    fRespReqd,
    tpPESession     psessionEntry)
{
    tpDeleteStaParams pDelStaParams = NULL;
    tSirMsgQ msgQ;
    tSirRetStatus     retCode = eSIR_SUCCESS;

    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **) &pDelStaParams, sizeof( tDeleteStaParams )))
    {
        limLog( pMac, LOGP, FL( "Unable to PAL allocate memory during ADD_STA\n" ));
        return eSIR_MEM_ALLOC_FAILED;
    }

    palZeroMemory( pMac->hHdd, (tANI_U8 *) pDelStaParams, sizeof(tDeleteStaParams));

  //
  // DPH contains the STA index only for "peer" STA entries.
  // LIM global contains "self" STA index
  // Thus,
  //    if( STA role )
  //      get STA index from LIM global
  //    else
  //      get STA index from DPH
  //

#if 0    
    /* Since we have not created any STA, no need to send msg to delete 
     * STA to HAL */
    if (psessionEntry->limSmeState == eLIM_SME_JOIN_FAILURE_STATE) {
        pDelStaParams->staIdx = 1; /* TODO : This is workaround. Need to find right STA Index before sending to HAL */
        //return retCode;
    }
#endif

#ifdef FEATURE_WLAN_TDLS
    if( ((eLIM_STA_ROLE == GET_LIM_SYSTEM_ROLE(psessionEntry)) && (pStaDs->staType !=  STA_ENTRY_TDLS_PEER)) ||(eLIM_BT_AMP_STA_ROLE == GET_LIM_SYSTEM_ROLE(psessionEntry)) )
#else
    if( (eLIM_STA_ROLE == GET_LIM_SYSTEM_ROLE(psessionEntry)) ||(eLIM_BT_AMP_STA_ROLE == GET_LIM_SYSTEM_ROLE(psessionEntry)) )
#endif
      pDelStaParams->staIdx= psessionEntry->staId;
    
    else
      pDelStaParams->staIdx= pStaDs->staIndex;

    pDelStaParams->assocId = pStaDs->assocId;
    pStaDs->valid             = 0;

    if (! fRespReqd)
        pDelStaParams->respReqd = 0;
    else
    {
        //when limDelSta is called from processSmeAssocCnf then mlmState is already set properly.
        if(eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE != GET_LIM_STA_CONTEXT_MLM_STATE(pStaDs))
        {
            MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, eLIM_MLM_WT_DEL_STA_RSP_STATE));
            SET_LIM_STA_CONTEXT_MLM_STATE(pStaDs, eLIM_MLM_WT_DEL_STA_RSP_STATE);
        }
        if ( (eLIM_STA_ROLE == GET_LIM_SYSTEM_ROLE(psessionEntry)) || 
             (eLIM_BT_AMP_STA_ROLE == GET_LIM_SYSTEM_ROLE(psessionEntry)) )
        {
            MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, eLIM_MLM_WT_DEL_STA_RSP_STATE));

            psessionEntry->limMlmState = eLIM_MLM_WT_DEL_STA_RSP_STATE; 
    
        }
        pDelStaParams->respReqd = 1;
        //we need to defer the message until we get the response back from HAL.
        SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
    }

    /* Update PE session ID*/
    pDelStaParams->sessionId = psessionEntry->peSessionId;
    
    pDelStaParams->status  = eHAL_STATUS_SUCCESS;
    msgQ.type = WDA_DELETE_STA_REQ;
    msgQ.reserved = 0;
    msgQ.bodyptr = pDelStaParams;
    msgQ.bodyval = 0;

    limLog( pMac, LOG1, FL( "Sending SIR_HAL_DELETE_STA_REQ for STAID: %X and AssocID: %d" ),
    pDelStaParams->staIdx, pDelStaParams->assocId);
    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));
    retCode = wdaPostCtrlMsg( pMac, &msgQ );
    if( eSIR_SUCCESS != retCode)
    {
        if(fRespReqd)
           SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
        limLog( pMac, LOGE, FL("Posting DELETE_STA_REQ to HAL failed, reason=%X\n"),
                        retCode );
        palFreeMemory(pMac->hHdd, (void*)pDelStaParams);
    }

    return retCode;
}

#if defined WLAN_FEATURE_VOWIFI_11R
/*------------------------------------------------------------------------
 * limAddFTStaSelf()
 *
 * FUNCTION:
 *
 * This function is called to add a STA once we have connected with a new
 * AP, that we have performed an FT to. 
 *
 * The Add STA Response is created and now after the ADD Bss Is Successful
 * we add the self sta. We update with the association id from the reassoc
 * response from the AP. 
 *------------------------------------------------------------------------
 */
tSirRetStatus limAddFTStaSelf(tpAniSirGlobal pMac, tANI_U16 assocId, tpPESession psessionEntry)
{
    tpAddStaParams pAddStaParams = NULL;
    tSirMsgQ msgQ;
    tSirRetStatus     retCode = eSIR_SUCCESS;

    pAddStaParams = pMac->ft.ftPEContext.pAddStaReq;
    pAddStaParams->assocId = assocId;

    msgQ.type = SIR_HAL_ADD_STA_REQ;
    msgQ.reserved = 0;
    msgQ.bodyptr = pAddStaParams;
    msgQ.bodyval = 0;


#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    limLog( pMac, LOGE, FL( "Sending SIR_HAL_ADD_STA_REQ... (aid %d)" ), pAddStaParams->assocId);
#endif
    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));

    psessionEntry->limPrevMlmState = psessionEntry->limMlmState;
    psessionEntry->limMlmState = eLIM_MLM_WT_ADD_STA_RSP_STATE;
    if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
    {
        limLog( pMac, LOGE, FL("Posting ADD_STA_REQ to HAL failed, reason=%X\n"), retCode );
        palFreeMemory(pMac->hHdd, (void*)pAddStaParams);
    }
    //
    // Dont need it any more
    pMac->ft.ftPEContext.pAddStaReq = NULL;
    return retCode;
}

#endif /* WLAN_FEATURE_VOWIFI_11R */

/**
 * limAddStaSelf()
 *
 *FUNCTION:
 * This function is called to add an STA context at hardware
 * whenever a STA is (Re) Associated.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  pStaDs  - Pointer to the STA datastructure created by
 *                   LIM and maintained by DPH
 * @return retCode - Indicates success or failure return code
 */

tSirRetStatus
limAddStaSelf(tpAniSirGlobal pMac,tANI_U16 staIdx, tANI_U8 updateSta, tpPESession psessionEntry)
{
    tpAddStaParams pAddStaParams = NULL;
    tSirMsgQ msgQ;
    tSirRetStatus     retCode = eSIR_SUCCESS;
    tSirMacAddr staMac;
    tANI_U32 listenInterval = WNI_CFG_LISTEN_INTERVAL_STADEF;

    #if 0
    retCode =wlan_cfgGetStr(pMac, WNI_CFG_STA_ID, staMac, &cfg);
    if (retCode != eSIR_SUCCESS)
        {
            /// Could not get BSSID from CFG. Log error.
            limLog(pMac, LOGP, FL("could not retrieve STA MAC\n"));
        return retCode;
        }
    #endif //TO SUPPORT BT-AMP
    sirCopyMacAddr(staMac,psessionEntry->selfMacAddr);

    if( eHAL_STATUS_SUCCESS !=
        palAllocateMemory( pMac->hHdd, (void **) &pAddStaParams, sizeof( tAddStaParams )))
    {
        limLog( pMac, LOGP, FL( "Unable to PAL allocate memory during ADD_STA\n" ));
        return eSIR_MEM_ALLOC_FAILED;
    }
    palZeroMemory( pMac->hHdd, (tANI_U8 *) pAddStaParams, sizeof(tAddStaParams));

    /// Add STA context at MAC HW (BMU, RHP & TFP)
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pAddStaParams->staMac,
                  (tANI_U8 *) staMac, sizeof(tSirMacAddr));

    palCopyMemory( pMac->hHdd, (tANI_U8 *) pAddStaParams->bssId,
                psessionEntry->bssId, sizeof(tSirMacAddr));

    pAddStaParams->assocId = psessionEntry->limAID;
    pAddStaParams->staType = STA_ENTRY_SELF;
    pAddStaParams->status = eHAL_STATUS_SUCCESS;
    pAddStaParams->respReqd = 1;

    /* Update  PE session ID */
    pAddStaParams->sessionId = psessionEntry->peSessionId;
    
  // This will indicate HAL to "allocate" a new STA index
    pAddStaParams->staIdx = staIdx;
    pAddStaParams->updateSta = updateSta;

    pAddStaParams->shortPreambleSupported = psessionEntry->beaconParams.fShortPreamble;

#ifdef WLAN_FEATURE_11AC
    limPopulateOwnRateSet(pMac, &pAddStaParams->supportedRates, NULL, false,psessionEntry,NULL);
#else
    limPopulateOwnRateSet(pMac, &pAddStaParams->supportedRates, NULL, false,psessionEntry);
#endif
    if( psessionEntry->htCapability)
    {
        pAddStaParams->htCapable = psessionEntry->htCapability;
#ifdef DISABLE_GF_FOR_INTEROP
        /*
         * To resolve the interop problem with Broadcom AP, 
         * where TQ STA could not pass traffic with GF enabled,
         * TQ STA will do Greenfield only with TQ AP, for 
         * everybody else it will be turned off.
        */
        if( (psessionEntry->pLimJoinReq != NULL) && (!psessionEntry->pLimJoinReq->bssDescription.aniIndicator))
        {
            limLog( pMac, LOGE, FL(" Turning off Greenfield, when adding self entry"));
            pAddStaParams->greenFieldCapable = WNI_CFG_GREENFIELD_CAPABILITY_DISABLE;
        }
        else
#endif

#ifdef WLAN_SOFTAP_FEATURE
        pAddStaParams->greenFieldCapable = limGetHTCapability( pMac, eHT_GREENFIELD, psessionEntry);
        pAddStaParams->txChannelWidthSet = limGetHTCapability( pMac, eHT_SUPPORTED_CHANNEL_WIDTH_SET, psessionEntry);
        pAddStaParams->mimoPS            = limGetHTCapability( pMac, eHT_MIMO_POWER_SAVE, psessionEntry );
        pAddStaParams->rifsMode          = limGetHTCapability( pMac, eHT_RIFS_MODE, psessionEntry );
        pAddStaParams->lsigTxopProtection = limGetHTCapability( pMac, eHT_LSIG_TXOP_PROTECTION, psessionEntry );
        pAddStaParams->delBASupport      = limGetHTCapability( pMac, eHT_DELAYED_BA, psessionEntry );
        pAddStaParams->maxAmpduDensity   = limGetHTCapability( pMac, eHT_MPDU_DENSITY, psessionEntry );
        pAddStaParams->maxAmpduSize   = limGetHTCapability(pMac, eHT_MAX_RX_AMPDU_FACTOR, psessionEntry);
        pAddStaParams->maxAmsduSize      = limGetHTCapability( pMac, eHT_MAX_AMSDU_LENGTH, psessionEntry );
        pAddStaParams->fDsssCckMode40Mhz = limGetHTCapability( pMac, eHT_DSSS_CCK_MODE_40MHZ, psessionEntry);
        pAddStaParams->fShortGI20Mhz     = limGetHTCapability( pMac, eHT_SHORT_GI_20MHZ, psessionEntry);
        pAddStaParams->fShortGI40Mhz     = limGetHTCapability( pMac, eHT_SHORT_GI_40MHZ, psessionEntry);
#else
        pAddStaParams->greenFieldCapable = limGetHTCapability( pMac, eHT_GREENFIELD );
        pAddStaParams->txChannelWidthSet = limGetHTCapability( pMac, eHT_SUPPORTED_CHANNEL_WIDTH_SET );
        pAddStaParams->mimoPS            = limGetHTCapability( pMac, eHT_MIMO_POWER_SAVE );
        pAddStaParams->rifsMode          = limGetHTCapability( pMac, eHT_RIFS_MODE );
        pAddStaParams->lsigTxopProtection = limGetHTCapability( pMac, eHT_LSIG_TXOP_PROTECTION );
        pAddStaParams->delBASupport      = limGetHTCapability( pMac, eHT_DELAYED_BA );
        pAddStaParams->maxAmpduDensity   = limGetHTCapability( pMac, eHT_MPDU_DENSITY );
        pAddStaParams->maxAmpduSize   = limGetHTCapability(pMac, eHT_MAX_RX_AMPDU_FACTOR);
        pAddStaParams->maxAmsduSize      = limGetHTCapability( pMac, eHT_MAX_AMSDU_LENGTH );
        pAddStaParams->fDsssCckMode40Mhz = limGetHTCapability( pMac, eHT_DSSS_CCK_MODE_40MHZ);
        pAddStaParams->fShortGI20Mhz     = limGetHTCapability( pMac, eHT_SHORT_GI_20MHZ);
        pAddStaParams->fShortGI40Mhz     = limGetHTCapability( pMac, eHT_SHORT_GI_40MHZ);
#endif
    }
#ifdef WLAN_FEATURE_11AC
    pAddStaParams->vhtCapable = psessionEntry->vhtCapability;
    pAddStaParams->vhtTxChannelWidthSet = psessionEntry->apChanWidth;
#endif
    if(wlan_cfgGetInt(pMac, WNI_CFG_LISTEN_INTERVAL, &listenInterval) != eSIR_SUCCESS)
       limLog(pMac, LOGP, FL("Couldn't get LISTEN_INTERVAL\n"));
    pAddStaParams->listenInterval = (tANI_U16)listenInterval;

#ifdef WLAN_FEATURE_P2P
    if (VOS_P2P_CLIENT_MODE == psessionEntry->pePersona)
    {
        pAddStaParams->p2pCapableSta = 1;       
    }
#endif

    limFillSupportedRatesInfo(pMac, NULL, &pAddStaParams->supportedRates,psessionEntry);

    msgQ.type = WDA_ADD_STA_REQ;
  //
  // FIXME_GEN4
  // A global counter (dialog token) is required to keep track of
  // all PE <-> HAL communication(s)
  //
    msgQ.reserved = 0;
    msgQ.bodyptr = pAddStaParams;
    msgQ.bodyval = 0;

    limLog( pMac, LOGW, FL( "Sending SIR_HAL_ADD_STA_REQ... (aid %d)" ),
          pAddStaParams->assocId);
  MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));

  if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
    {
        limLog( pMac, LOGE, FL("Posting ADD_STA_REQ to HAL failed, reason=%X\n"), retCode );
        palFreeMemory(pMac->hHdd, (void*)pAddStaParams);
    }
      return retCode;
}


/**
 * limTeardownInfraBSS()
 *
 *FUNCTION:
 * This function is called by various LIM functions to teardown
 * an established Infrastructure BSS
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limTeardownInfraBss(tpAniSirGlobal pMac,tpPESession psessionEntry)
{
    tSirMacAddr   bcAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        /**
         * Send Broadcast Disassociate frame with
         * 'leaving BSS' reason.
         */
        limSendDisassocMgmtFrame(pMac,
                                 eSIR_MAC_DISASSOC_LEAVING_BSS_REASON,
                                 bcAddr,psessionEntry, FALSE);
} /*** end limTeardownInfraBss() ***/


/**
 * limHandleCnfWaitTimeout()
 *
 *FUNCTION:
 * This function is called by limProcessMessageQueue to handle
 * various confirmation failure cases.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pStaDs - Pointer to a sta descriptor
 * @return None
 */

void limHandleCnfWaitTimeout(tpAniSirGlobal pMac, tANI_U16 staId)
{
    tpDphHashNode       pStaDs;
    tLimSystemRole systemRole;
    tpPESession psessionEntry = NULL;

    if((psessionEntry = peFindSessionBySessionId(pMac, pMac->lim.limTimers.gpLimCnfWaitTimer[staId].sessionId))== NULL) 
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        return;
    }
    systemRole = limGetSystemRole(psessionEntry);
    pStaDs = dphGetHashEntry(pMac, staId, &psessionEntry->dph.dphHashTable);

    if (pStaDs == NULL)
    {
        PELOGW(limLog(pMac, LOGW, FL("No STA context in SIR_LIM_CNF_WAIT_TIMEOUT.\n"));)
        return;
    }

    switch (pStaDs->mlmStaContext.mlmState) {
        case eLIM_MLM_WT_ASSOC_CNF_STATE:
            PELOGW(limLog(pMac, LOGW, FL("Did not receive Assoc Cnf in eLIM_MLM_WT_ASSOC_CNF_STATE sta Assoc id %d\n"), pStaDs->assocId);)
            limPrintMacAddr(pMac, pStaDs->staAddr, LOGW);

            if ( (systemRole == eLIM_AP_ROLE)|| (systemRole == eLIM_BT_AMP_AP_ROLE) )
            {
                limRejectAssociation(
                            pMac,
                            pStaDs->staAddr,
                            pStaDs->mlmStaContext.subType,
                            true,
                            pStaDs->mlmStaContext.authType,
                            pStaDs->assocId,
                            true,
                            (tSirResultCodes) eSIR_MAC_UNSPEC_FAILURE_STATUS,
                            psessionEntry);
            }
            break;

        default:
            limLog(pMac, LOGW, FL("Received CNF_WAIT_TIMEOUT in state %d\n"),
                   pStaDs->mlmStaContext.mlmState);
    }
}


/**
 * limDeleteDphHashEntry()
 *
 *FUNCTION:
 * This function is called whenever we need to delete
 * the dph hash entry
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  tANI_U16       staId
 * @return None
 */

void
limDeleteDphHashEntry(tpAniSirGlobal pMac, tSirMacAddr staAddr, tANI_U16 staId,tpPESession psessionEntry)
{
    tANI_U16              aid;
    tpDphHashNode    pStaDs;
    tUpdateBeaconParams beaconParams;    
    tLimSystemRole systemRole;

    beaconParams.paramChangeBitmap = 0;
    limDeactivateAndChangePerStaIdTimer(pMac, eLIM_CNF_WAIT_TIMER, staId);
    if (NULL == psessionEntry)
    {
        PELOGE(limLog(pMac, LOGE, FL("NULL psessionEntry"));)
        return;
    }
    systemRole = limGetSystemRole(psessionEntry);
    beaconParams.bssIdx = psessionEntry->bssIdx;
    pStaDs = dphLookupHashEntry(pMac, staAddr, &aid, &psessionEntry->dph.dphHashTable);
    if (pStaDs != NULL)
    {
         PELOGW(limLog(pMac, LOGW, FL("Deleting DPH Hash entry for STAID: %X\n "), staId);)
        // update the station count and perform associated actions
        // do this before deleting the dph hash entry
        limUtilCountStaDel(pMac, pStaDs, psessionEntry);

        if((eLIM_AP_ROLE == psessionEntry->limSystemRole) ||
              (eLIM_STA_IN_IBSS_ROLE == psessionEntry->limSystemRole))
        {
#ifdef WLAN_SOFTAP_FEATURE
            if(psessionEntry->limSystemRole == eLIM_AP_ROLE ){
                if(psessionEntry->gLimProtectionControl != WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE){
                limDecideApProtectionOnDelete(pMac, pStaDs, &beaconParams,psessionEntry);
                }
            }
#endif         

            if(eLIM_STA_IN_IBSS_ROLE == systemRole)
                limIbssDecideProtectionOnDelete(pMac, pStaDs, &beaconParams, psessionEntry);

            limDecideShortPreamble(pMac, pStaDs, &beaconParams, psessionEntry);
            limDecideShortSlot(pMac, pStaDs, &beaconParams, psessionEntry);

            //Send message to HAL about beacon parameter change.
            PELOGW(limLog(pMac, LOGW, FL("param bitmap = %d \n"), beaconParams.paramChangeBitmap);)
            if(beaconParams.paramChangeBitmap)
            {
                schSetFixedBeaconFields(pMac,psessionEntry);    
                limSendBeaconParams(pMac, &beaconParams, psessionEntry );
            }
        }
        if (dphDeleteHashEntry(pMac, staAddr, staId, &psessionEntry->dph.dphHashTable) != eSIR_SUCCESS)
           limLog(pMac, LOGP, FL("error deleting hash entry\n"));
    }
}



/**
 * limCheckAndAnnounceJoinSuccess()
 *
 *FUNCTION:
 * This function is called upon receiving Beacon/Probe Response
 * frame in WT_JOIN_BEACON_STATE to check if the received
 * Beacon/Probe Response is from the BSS that we're attempting
 * to join.
 *
 *LOGIC:
 * If the Beacon/Probe Response is indeed from the BSS we're
 * attempting to join, join success is sent to SME.
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  pBPR      Pointer to received Beacon/Probe Response
 * @param  pHdr      Pointer to received Beacon/Probe Response
 *                   MAC header
 * @return None
 */

void
limCheckAndAnnounceJoinSuccess(tpAniSirGlobal pMac,
                               tSirProbeRespBeacon *pBPR,
                               tpSirMacMgmtHdr pHdr,tpPESession psessionEntry)
{
    tSirMacSSid          currentSSID;
    tLimMlmJoinCnf       mlmJoinCnf;
#ifdef WLAN_FEATURE_P2P
    tANI_U32             val = 0;
    tANI_U32             *noa1DurationFromBcn = NULL;
    tANI_U32             *noa2DurationFromBcn = NULL;
    tANI_U32             noa;
    tANI_U32             TotalNum_NoADesc = 0;
#endif

    palCopyMemory( pMac->hHdd, currentSSID.ssId,
                       psessionEntry->ssId.ssId,
                       psessionEntry->ssId.length);
   
    currentSSID.length = (tANI_U8)psessionEntry->ssId.length ;

    if (
        /* Check for SSID only in probe response. Beacons may not carry
           SSID information in hidden SSID case */
        ( (SIR_MAC_MGMT_FRAME  == pHdr->fc.type) &&
          (SIR_MAC_MGMT_PROBE_RSP == pHdr->fc.subType) ) &&
        currentSSID.length &&
        (!palEqualMemory( pMac->hHdd,(tANI_U8 *) &pBPR->ssId,
                   (tANI_U8 *) &currentSSID,
                   (tANI_U8) (1 + currentSSID.length)) ))
    {
        /**
         * Received SSID does not match with the one we've.
         * Ignore received Beacon frame
         */
        PELOG1(limLog(pMac, LOG1, FL("SSID received in Beacon does not match\n"));)
#ifdef WLAN_DEBUG            
        pMac->lim.gLimBcnSSIDMismatchCnt++;
#endif
        return;
    }

    if( (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE)||(psessionEntry->limSystemRole == eLIM_STA_ROLE))
    {
        PELOG1(limLog(pMac, LOG1, FL("Received Beacon/PR with matching BSSID\n"));)

        // Deactivate Join Failure timer
        limDeactivateAndChangeTimer(pMac, eLIM_JOIN_FAIL_TIMER);
        // Deactivate Periodic Join timer
        limDeactivateAndChangeTimer(pMac, eLIM_PERIODIC_JOIN_PROBE_REQ_TIMER);

#ifdef WLAN_FEATURE_P2P
    if (VOS_P2P_CLIENT_MODE == psessionEntry->pePersona &&
                                   pBPR->P2PProbeRes.NoticeOfAbsence.present)
    {

        noa1DurationFromBcn = (tANI_U32*)(pBPR->P2PProbeRes.NoticeOfAbsence.NoADesc + 1);

        if(pBPR->P2PProbeRes.NoticeOfAbsence.num_NoADesc)
            TotalNum_NoADesc = pBPR->P2PProbeRes.NoticeOfAbsence.num_NoADesc/SIZE_OF_NOA_DESCRIPTOR;

        noa = *noa1DurationFromBcn;

        if(TotalNum_NoADesc > 1)
        {
            noa2DurationFromBcn = (tANI_U32*)(pBPR->P2PProbeRes.NoticeOfAbsence.NoADesc + SIZE_OF_NOA_DESCRIPTOR + 1);
            noa += *noa2DurationFromBcn;
        }

        /*If MAX Noa exceeds 3 secs we will consider only 3 secs to 
        * avoid arbitary values in noa duration field
        */
        noa = noa >  MAX_NOA_PERIOD_IN_MICROSECS ? MAX_NOA_PERIOD_IN_MICROSECS : noa;
        noa = noa/1000; //Convert to ms

        if( wlan_cfgGetInt(pMac, WNI_CFG_AUTHENTICATE_FAILURE_TIMEOUT,&val) == eSIR_SUCCESS )
        {
            psessionEntry->defaultAuthFailureTimeout = val;
            ccmCfgSetInt(pMac,WNI_CFG_AUTHENTICATE_FAILURE_TIMEOUT ,val + noa, NULL, eANI_BOOLEAN_FALSE);
        }
    }
    else
    {
        psessionEntry->defaultAuthFailureTimeout = 0;
    }
#endif

        // Update Beacon Interval at CFG database

        if ( pBPR->HTCaps.present )
            limUpdateStaRunTimeHTCapability( pMac, &pBPR->HTCaps );
        if ( pBPR->HTInfo.present )
            limUpdateStaRunTimeHTInfo( pMac, &pBPR->HTInfo, psessionEntry);
        psessionEntry->limMlmState = eLIM_MLM_JOINED_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, eLIM_MLM_JOINED_STATE));

#if (WNI_POLARIS_FW_PRODUCT == AP)
        // In case of BP, we need to adopt to all rates
        // advertised by AP. Update the operational rates at CFG
    if (cfgSetStr(pMac, WNI_CFG_OPERATIONAL_RATE_SET,
                      (tANI_U8 *) &pBPR->supportedRates.rate,
                      pBPR->supportedRates.numRates)
            != eSIR_SUCCESS)
            limLog(pMac, LOGP, FL("could not update Oper.rates at CFG\n"));
       
#endif

        /**
         * Announce join success by sending
         * Join confirm to SME.
         */
        mlmJoinCnf.resultCode = eSIR_SME_SUCCESS;
        mlmJoinCnf.protStatusCode = eSIR_MAC_SUCCESS_STATUS;
        /* Update PE sessionId*/
        mlmJoinCnf.sessionId = psessionEntry->peSessionId;
        limPostSmeMessage(pMac, LIM_MLM_JOIN_CNF, (tANI_U32 *) &mlmJoinCnf);
    } // if ((pMac->lim.gLimSystemRole == IBSS....
}

/**
 * limExtractApCapabilities()
 *
 *FUNCTION:
 * This function is called to extract all of the AP's capabilities
 * from the IEs received from it in Beacon/Probe Response frames
 *
 *LOGIC:
 * This routine mimics the limExtractApCapability() API. The difference here
 * is that this API returns the entire tSirProbeRespBeacon info as is. It is
 * left to the caller of this API to use this info as required
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param   pMac         Pointer to Global MAC structure
 * @param   pIE          Pointer to starting IE in Beacon/Probe Response
 * @param   ieLen        Length of all IEs combined
 * @param   beaconStruct A pointer to tSirProbeRespBeacon that needs to be
 *                       populated
 * @return  status       A status reporting eSIR_SUCCESS or eSIR_FAILURE
 */
tSirRetStatus limExtractApCapabilities( tpAniSirGlobal pMac,
    tANI_U8 *pIE,
    tANI_U16 ieLen,
    tpSirProbeRespBeacon beaconStruct )
{
  palZeroMemory( pMac->hHdd, (tANI_U8 *) beaconStruct, sizeof( tSirProbeRespBeacon ));

  PELOG3(limLog( pMac, LOG3,
      FL( "In limExtractApCapabilities: The IE's being received are:\n" ));
  sirDumpBuf( pMac, SIR_LIM_MODULE_ID, LOG3, pIE, ieLen );)

  // Parse the Beacon IE's, Don't try to parse if we dont have anything in IE
  if (ieLen > 0) {
  if( eSIR_SUCCESS != sirParseBeaconIE( pMac, beaconStruct, pIE, (tANI_U32)ieLen ))
  {
    limLog( pMac, LOGE, FL("APCapExtract: Beacon parsing error!\n"));
    return eSIR_FAILURE;
  }
  }

  return eSIR_SUCCESS;
}


/**
 * limDelBss()
 *
 *FUNCTION:
 * This function is called to delete BSS context at hardware
 * whenever a STA is disassociated
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  pStaDs  - Pointer to the STA datastructure created by
 *                   LIM and maintained by DPH
 * @return retCode - Indicates success or failure return code
 */

tSirRetStatus
limDelBss(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tANI_U16 bssIdx,tpPESession psessionEntry)
{
    tpDeleteBssParams pDelBssParams = NULL;
    tSirMsgQ msgQ;
    tSirRetStatus     retCode = eSIR_SUCCESS;
    
     if( eHAL_STATUS_SUCCESS !=
        palAllocateMemory( pMac->hHdd, (void **) &pDelBssParams, sizeof( tDeleteBssParams )))
     {
        limLog( pMac, LOGP, FL( "Unable to PAL allocate memory during ADD_BSS\n" ));
        return eSIR_MEM_ALLOC_FAILED;
     }
     palZeroMemory( pMac->hHdd, (tANI_U8 *) pDelBssParams, sizeof(tDeleteBssParams));


    pDelBssParams->sessionId = psessionEntry->peSessionId; //update PE session Id

    //DPH was storing the AssocID in staID field,
    //staID is actually assigned by HAL when AddSTA message is sent.
    if (pStaDs != NULL)
    {
        pDelBssParams->bssIdx= pStaDs->bssId;
        pStaDs->valid                  = 0;
        pStaDs->mlmStaContext.mlmState = eLIM_MLM_WT_DEL_BSS_RSP_STATE;
    }
    else
        pDelBssParams->bssIdx          = bssIdx;
    psessionEntry->limMlmState = eLIM_MLM_WT_DEL_BSS_RSP_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, eLIM_MLM_WT_DEL_BSS_RSP_STATE));

    pDelBssParams->status= eHAL_STATUS_SUCCESS;
    pDelBssParams->respReqd = 1;
   PELOGW(limLog( pMac, LOGW, FL("Sending HAL_DELETE_BSS_REQ for BSSID: %X\n"),
          pDelBssParams->bssIdx);)

    //we need to defer the message until we get the response back from HAL.
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);

    msgQ.type = WDA_DELETE_BSS_REQ;
    msgQ.reserved = 0;
    msgQ.bodyptr = pDelBssParams;
    msgQ.bodyval = 0;

    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));

    if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
    {
        SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
        limLog( pMac, LOGE, FL("Posting DELETE_BSS_REQ to HAL failed, reason=%X\n"), retCode );
        palFreeMemory(pMac->hHdd, (void*)pDelBssParams);
    }

    return retCode;
}


#ifdef ANI_PRODUCT_TYPE_CLIENT

/**
 * limSendAddBss()
 *
 *FUNCTION:
 *
 *LOGIC:
 * 1) LIM receives eWNI_SME_JOIN_REQ
 * 2) For a valid eWNI_SME_JOIN_REQ, LIM sends
 * SIR_HAL_ADD_BSS_REQ to HAL
 *
 *ASSUMPTIONS:
 * JOIN REQ parameters are saved in pMac->lim.gLimMlmJoinReq
 * ADD BSS parameters can be obtained from two sources:
 * 1) pMac->lim.gLimMlmJoinReq
 * 2) beaconStruct, passed as paramter
 * So, if a reqd parameter is found in bssDescriptions
 * then it is given preference over beaconStruct
 *
 *NOTE:
 *
 * @param  pMac Pointer to Global MAC structure
 *              pAssocRsp    contains the structured assoc/reassoc Response got from AP
 *              beaconstruct        Has the ProbeRsp/Beacon structured details
 *              bssDescription      bssDescription passed to PE from the SME
 * @return None
 */

tSirRetStatus limStaSendAddBss( tpAniSirGlobal pMac, tpSirAssocRsp pAssocRsp,
    tpSchBeaconStruct pBeaconStruct, tpSirBssDescription bssDescription, tANI_U8 updateEntry,
    tpPESession psessionEntry)

{
    tSirMsgQ msgQ;
    tpAddBssParams pAddBssParams = NULL;
    tANI_U32 retCode;
    tANI_U8 i;
    tpDphHashNode pStaDs = NULL;
    tANI_U8 chanWidthSupp = 0;
    // Package SIR_HAL_ADD_BSS_REQ message parameters
    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
                                                  (void **) &pAddBssParams,
                                                  sizeof( tAddBssParams )))
    {
        limLog( pMac, LOGP,
                FL( "Unable to PAL allocate memory during ADD_BSS\n" ));
        retCode = eSIR_MEM_ALLOC_FAILED;
        goto returnFailure;
    }
    else
        palZeroMemory( pMac->hHdd, (tANI_U8 *) pAddBssParams, sizeof( tAddBssParams ));

    palCopyMemory( pMac->hHdd,  pAddBssParams->bssId,bssDescription->bssId,
                   sizeof( tSirMacAddr ));
    // Fill in tAddBssParams selfMacAddr
    palCopyMemory( pMac->hHdd,  pAddBssParams->selfMacAddr, 
                   psessionEntry->selfMacAddr,
                   sizeof( tSirMacAddr ));
    
    if(psessionEntry->bssType == eSIR_BTAMP_AP_MODE)
    {
         pAddBssParams->bssType = eSIR_BTAMP_AP_MODE;
    }
    else
    {
        pAddBssParams->bssType = eSIR_INFRASTRUCTURE_MODE;
    }
    
    pAddBssParams->operMode = BSS_OPERATIONAL_MODE_STA;

    /* Update PE session ID */
    pAddBssParams->sessionId = psessionEntry->peSessionId;

    pAddBssParams->beaconInterval = bssDescription->beaconInterval;
    
    pAddBssParams->dtimPeriod = pBeaconStruct->tim.dtimPeriod;
    pAddBssParams->updateBss = updateEntry;


    pAddBssParams->cfParamSet.cfpCount = pBeaconStruct->cfParamSet.cfpCount;
    pAddBssParams->cfParamSet.cfpPeriod = pBeaconStruct->cfParamSet.cfpPeriod;
    pAddBssParams->cfParamSet.cfpMaxDuration = pBeaconStruct->cfParamSet.cfpMaxDuration;
    pAddBssParams->cfParamSet.cfpDurRemaining = pBeaconStruct->cfParamSet.cfpDurRemaining;

    pAddBssParams->rateSet.numRates = pAssocRsp->supportedRates.numRates;
    palCopyMemory( pMac->hHdd,  pAddBssParams->rateSet.rate,
                   pAssocRsp->supportedRates.rate, pAssocRsp->supportedRates.numRates );

    pAddBssParams->nwType = bssDescription->nwType;
    
    pAddBssParams->shortSlotTimeSupported = (tANI_U8)pAssocRsp->capabilityInfo.shortSlotTime;
    pAddBssParams->llaCoexist = (tANI_U8) psessionEntry->beaconParams.llaCoexist;    
    pAddBssParams->llbCoexist = (tANI_U8) psessionEntry->beaconParams.llbCoexist;
    pAddBssParams->llgCoexist = (tANI_U8) psessionEntry->beaconParams.llgCoexist;
    pAddBssParams->ht20Coexist = (tANI_U8) psessionEntry->beaconParams.ht20Coexist;   


    // Use the advertised capabilities from the received beacon/PR


    if (IS_DOT11_MODE_HT(psessionEntry->dot11mode) && ( pAssocRsp->HTCaps.present ))
    {
        pAddBssParams->htCapable = pAssocRsp->HTCaps.present;

        if ( pBeaconStruct->HTInfo.present )
        {
            pAddBssParams->htOperMode = (tSirMacHTOperatingMode)pAssocRsp->HTInfo.opMode;
            pAddBssParams->dualCTSProtection = ( tANI_U8 ) pAssocRsp->HTInfo.dualCTSProtection;
#ifdef WLAN_SOFTAP_FEATURE
            chanWidthSupp = limGetHTCapability( pMac, eHT_SUPPORTED_CHANNEL_WIDTH_SET, psessionEntry);
#else
            chanWidthSupp = limGetHTCapability( pMac, eHT_SUPPORTED_CHANNEL_WIDTH_SET);
#endif
            if( (pAssocRsp->HTCaps.supportedChannelWidthSet) &&
                (chanWidthSupp) )
            {
                pAddBssParams->txChannelWidthSet = ( tANI_U8 )pAssocRsp->HTInfo.recommendedTxWidthSet;
                pAddBssParams->currentExtChannel = pAssocRsp->HTInfo.secondaryChannelOffset;
            }
            else
            {
                pAddBssParams->txChannelWidthSet = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
                pAddBssParams->currentExtChannel = PHY_SINGLE_CHANNEL_CENTERED;
            }
            pAddBssParams->llnNonGFCoexist = (tANI_U8)pAssocRsp->HTInfo.nonGFDevicesPresent;
            pAddBssParams->fLsigTXOPProtectionFullSupport = (tANI_U8)pAssocRsp->HTInfo.lsigTXOPProtectionFullSupport;
            pAddBssParams->fRIFSMode = pAssocRsp->HTInfo.rifsMode;
        }
    }

    pAddBssParams->currentOperChannel = bssDescription->channelId;

#ifdef WLAN_FEATURE_11AC
    if (psessionEntry->vhtCapability && ( pAssocRsp->VHTCaps.present ))
    {
        pAddBssParams->vhtCapable = pAssocRsp->VHTCaps.present;
        pAddBssParams->vhtTxChannelWidthSet = pAssocRsp->VHTOperation.chanWidth; 
        pAddBssParams->currentExtChannel = limGet11ACPhyCBState ( pMac, 
                                                                  pAddBssParams->currentOperChannel,
                                                                  pAddBssParams->currentExtChannel,
                                                                  psessionEntry->apCenterChan,
                                                                  psessionEntry);
    }
    else 
    {
        pAddBssParams->vhtCapable = 0;
    }
#endif


    // Populate the STA-related parameters here
    // Note that the STA here refers to the AP
    {
        /* staType = PEER*/
        pAddBssParams->staContext.staType = STA_ENTRY_OTHER; // Identifying AP as an STA

        palCopyMemory( pMac->hHdd,  pAddBssParams->staContext.bssId,
                       bssDescription->bssId,
                       sizeof( tSirMacAddr ));
        pAddBssParams->staContext.listenInterval = bssDescription->beaconInterval;

        /* Fill Assoc id from the dph table */
        pStaDs = dphLookupHashEntry(pMac, pAddBssParams->staContext.bssId, 
                &pAddBssParams->staContext.assocId, &psessionEntry->dph.dphHashTable);
        if (pStaDs == NULL)
        {
            PELOGE(limLog(pMac, LOGE, FL("Couldn't get assoc id for"
                       "MAC ADDR: %02x:%02x:%02x:%02x:%02x:%02x"),
                       pAddBssParams->staContext.staMac[0],
                       pAddBssParams->staContext.staMac[1],
                       pAddBssParams->staContext.staMac[2],
                       pAddBssParams->staContext.staMac[3],
                       pAddBssParams->staContext.staMac[4],
                       pAddBssParams->staContext.staMac[5]
                       );)
        }
        pAddBssParams->staContext.uAPSD = 0;
        pAddBssParams->staContext.maxSPLen = 0;
        pAddBssParams->staContext.shortPreambleSupported = (tANI_U8)pAssocRsp->capabilityInfo.shortPreamble;
        pAddBssParams->staContext.updateSta = updateEntry;

        if (IS_DOT11_MODE_HT(psessionEntry->dot11mode) && ( pBeaconStruct->HTCaps.present ))
        {
            pAddBssParams->staContext.us32MaxAmpduDuration = 0;
            pAddBssParams->staContext.htCapable = 1;
            pAddBssParams->staContext.greenFieldCapable  = ( tANI_U8 )pAssocRsp->HTCaps.greenField;
            pAddBssParams->staContext.lsigTxopProtection = ( tANI_U8 )pAssocRsp->HTCaps.lsigTXOPProtection;
            if( (pAssocRsp->HTCaps.supportedChannelWidthSet) &&
                (chanWidthSupp) )
            {
                pAddBssParams->staContext.txChannelWidthSet = ( tANI_U8 )pAssocRsp->HTInfo.recommendedTxWidthSet;
                
#ifdef WLAN_FEATURE_11AC
                if (psessionEntry->vhtCapability && ( pBeaconStruct->VHTCaps.present ))
                {
                    pAddBssParams->staContext.vhtCapable = 1;
                    pAddBssParams->staContext.vhtTxChannelWidthSet = pAssocRsp->VHTOperation.chanWidth; //pMac->lim.apChanWidth;
                }
#endif
            }
            else
            {
                pAddBssParams->staContext.txChannelWidthSet = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
            }                                                           
            pAddBssParams->staContext.mimoPS             = (tSirMacHTMIMOPowerSaveState)pAssocRsp->HTCaps.mimoPowerSave;
            pAddBssParams->staContext.delBASupport       = ( tANI_U8 )pAssocRsp->HTCaps.delayedBA;
            pAddBssParams->staContext.maxAmsduSize       = ( tANI_U8 )pAssocRsp->HTCaps.maximalAMSDUsize;
            pAddBssParams->staContext.maxAmpduDensity    =            pAssocRsp->HTCaps.mpduDensity;
            pAddBssParams->staContext.fDsssCckMode40Mhz = (tANI_U8)pAssocRsp->HTCaps.dsssCckMode40MHz;
            pAddBssParams->staContext.fShortGI20Mhz = (tANI_U8)pAssocRsp->HTCaps.shortGI20MHz;
            pAddBssParams->staContext.fShortGI40Mhz = (tANI_U8)pAssocRsp->HTCaps.shortGI40MHz;
            pAddBssParams->staContext.maxAmpduSize= pAssocRsp->HTCaps.maxRxAMPDUFactor;
            
            if( pBeaconStruct->HTInfo.present )
                pAddBssParams->staContext.rifsMode = pAssocRsp->HTInfo.rifsMode;
        }

        //If WMM IE or 802.11E IE is not present and AP is HT AP then enable WMM
        if ((psessionEntry->limWmeEnabled && (pAssocRsp->wmeEdcaPresent || pAddBssParams->htCapable)) ||
                (psessionEntry->limQosEnabled && (pAssocRsp->edcaPresent || pAddBssParams->htCapable)))
            pAddBssParams->staContext.wmmEnabled = 1;
        else 
            pAddBssParams->staContext.wmmEnabled = 0;

        //Update the rates

        pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);
        if (pStaDs != NULL)
        {
            limFillSupportedRatesInfo(pMac, pStaDs, &pStaDs->supportedRates,psessionEntry);
            palCopyMemory(pMac->hHdd, (tANI_U8*)&pAddBssParams->staContext.supportedRates,
                                                (tANI_U8*)&pStaDs->supportedRates,
                                                sizeof(tSirSupportedRates));
        }
        else
            PELOGE(limLog(pMac, LOGE, FL("could not Update the supported rates.\n"));)

    }

    //Disable BA. It will be set as part of ADDBA negotiation.
    for( i = 0; i < STACFG_MAX_TC; i++ )
    {
        pAddBssParams->staContext.staTCParams[i].txUseBA    = eBA_DISABLE;
        pAddBssParams->staContext.staTCParams[i].rxUseBA    = eBA_DISABLE;
        pAddBssParams->staContext.staTCParams[i].txBApolicy = eBA_POLICY_IMMEDIATE;
        pAddBssParams->staContext.staTCParams[i].rxBApolicy = eBA_POLICY_IMMEDIATE;
    }

    pAddBssParams->staContext.encryptType =  psessionEntry->encryptType;

#if defined WLAN_FEATURE_VOWIFI  
    pAddBssParams->maxTxPower = psessionEntry->maxTxPower;
#endif
    // FIXME_GEN4 - Any other value that can be used for initialization?
    pAddBssParams->status = eHAL_STATUS_SUCCESS;
    pAddBssParams->respReqd = true;

    pAddBssParams->halPersona = (tANI_U8)psessionEntry->pePersona; //update persona

#ifdef WLAN_FEATURE_P2P
    if (VOS_P2P_CLIENT_MODE == psessionEntry->pePersona)
    {
        pAddBssParams->staContext.p2pCapableSta = 1;       
    }
#endif

    pAddBssParams->bSpectrumMgtEnabled = psessionEntry->spectrumMgtEnabled || 
        limIsconnectedOnDFSChannel(bssDescription->channelId);

#if defined WLAN_FEATURE_VOWIFI_11R
    pAddBssParams->extSetStaKeyParamValid = 0;
#endif

    // Set a new state for MLME
    if( eLIM_MLM_WT_ASSOC_RSP_STATE == psessionEntry->limMlmState )
        psessionEntry->limMlmState = eLIM_MLM_WT_ADD_BSS_RSP_ASSOC_STATE;
    else
        psessionEntry->limMlmState = eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));

    //we need to defer the message until we get the response back from HAL.
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);

    msgQ.type = WDA_ADD_BSS_REQ;
    /** @ToDo : Update the Global counter to keeptrack of the PE <--> HAL messages*/
    msgQ.reserved = 0;
    msgQ.bodyptr = pAddBssParams;
    msgQ.bodyval = 0;

    limLog( pMac, LOG1, FL( "Sending SIR_HAL_ADD_BSS_REQ..." ));
    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));

    retCode = wdaPostCtrlMsg( pMac, &msgQ );
    if( eSIR_SUCCESS != retCode) 
    {
        SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
        palFreeMemory(pMac->hHdd, pAddBssParams);
        limLog( pMac, LOGE, FL("Posting ADD_BSS_REQ to HAL failed, reason=%X\n"),
                retCode );
        goto returnFailure;

    }
    else
        return retCode;

 returnFailure:
    // Clean-up will be done by the caller...
    return retCode;
}




tSirRetStatus limStaSendAddBssPreAssoc( tpAniSirGlobal pMac, tANI_U8 updateEntry, tpPESession psessionEntry)
{
    tSirMsgQ msgQ;
    tpAddBssParams pAddBssParams = NULL;
    tANI_U32 retCode;
    tANI_U8 i;
    tSchBeaconStruct *pBeaconStruct;
    tANI_U8 chanWidthSupp = 0;
    tpSirBssDescription bssDescription = &psessionEntry->pLimJoinReq->bssDescription;

    if(eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd, 
                                                (void **)&pBeaconStruct, sizeof(tSchBeaconStruct)))
    {
        limLog(pMac, LOGE, FL("Unable to PAL allocate memory during ADD_BSS\n") );
        return eSIR_MEM_ALLOC_FAILED;
    }


    // Package SIR_HAL_ADD_BSS_REQ message parameters
    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
                                                  (void **) &pAddBssParams,
                                                  sizeof( tAddBssParams )))
    {
        limLog( pMac, LOGP,
                FL( "Unable to PAL allocate memory during ADD_BSS\n" ));
        retCode = eSIR_MEM_ALLOC_FAILED;
        goto returnFailure;
    }
    
    palZeroMemory( pMac->hHdd, (tANI_U8 *) pAddBssParams, sizeof( tAddBssParams ));


    limExtractApCapabilities( pMac,
                            (tANI_U8 *) bssDescription->ieFields,
                            limGetIElenFromBssDescription( bssDescription ),
                            pBeaconStruct );

    if(pMac->lim.gLimProtectionControl != WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
        limDecideStaProtectionOnAssoc(pMac, pBeaconStruct, psessionEntry);
        palCopyMemory( pMac->hHdd,  pAddBssParams->bssId,bssDescription->bssId,
                   sizeof( tSirMacAddr ));

    // Fill in tAddBssParams selfMacAddr
    palCopyMemory( pMac->hHdd,  pAddBssParams->selfMacAddr, 
                   psessionEntry->selfMacAddr,
                   sizeof( tSirMacAddr ));

    /* Incorrect BSS Type which caused UMA Descriptor to be overwritten on
     * top of an already established Infra link. This lead to issues in 
     * concurrent data transfer.
     */

    pAddBssParams->bssType = psessionEntry->bssType;//eSIR_INFRASTRUCTURE_MODE;
    pAddBssParams->operMode = BSS_OPERATIONAL_MODE_STA;

    pAddBssParams->beaconInterval = bssDescription->beaconInterval;
    
    pAddBssParams->dtimPeriod = pBeaconStruct->tim.dtimPeriod;
    pAddBssParams->updateBss = updateEntry;


    pAddBssParams->cfParamSet.cfpCount = pBeaconStruct->cfParamSet.cfpCount;
    pAddBssParams->cfParamSet.cfpPeriod = pBeaconStruct->cfParamSet.cfpPeriod;
    pAddBssParams->cfParamSet.cfpMaxDuration = pBeaconStruct->cfParamSet.cfpMaxDuration;
    pAddBssParams->cfParamSet.cfpDurRemaining = pBeaconStruct->cfParamSet.cfpDurRemaining;


    pAddBssParams->rateSet.numRates = pBeaconStruct->supportedRates.numRates;
    palCopyMemory( pMac->hHdd,  pAddBssParams->rateSet.rate,
                   pBeaconStruct->supportedRates.rate, pBeaconStruct->supportedRates.numRates );

    pAddBssParams->nwType = bssDescription->nwType;
    
    pAddBssParams->shortSlotTimeSupported = (tANI_U8)pBeaconStruct->capabilityInfo.shortSlotTime; 
    pAddBssParams->llaCoexist = (tANI_U8) psessionEntry->beaconParams.llaCoexist;
    pAddBssParams->llbCoexist = (tANI_U8) psessionEntry->beaconParams.llbCoexist;
    pAddBssParams->llgCoexist = (tANI_U8) psessionEntry->beaconParams.llgCoexist;
    pAddBssParams->ht20Coexist = (tANI_U8) psessionEntry->beaconParams.ht20Coexist; 

    // Use the advertised capabilities from the received beacon/PR
    if (IS_DOT11_MODE_HT(psessionEntry->dot11mode) && ( pBeaconStruct->HTCaps.present ))
    {
        pAddBssParams->htCapable = pBeaconStruct->HTCaps.present;

        if ( pBeaconStruct->HTInfo.present )
        {
            pAddBssParams->htOperMode = (tSirMacHTOperatingMode)pBeaconStruct->HTInfo.opMode;
            pAddBssParams->dualCTSProtection = ( tANI_U8 ) pBeaconStruct->HTInfo.dualCTSProtection;

#ifdef WLAN_SOFTAP_FEATURE
            chanWidthSupp = limGetHTCapability( pMac, eHT_SUPPORTED_CHANNEL_WIDTH_SET, psessionEntry);
#else 
            chanWidthSupp = limGetHTCapability( pMac, eHT_SUPPORTED_CHANNEL_WIDTH_SET);
#endif
            if( (pBeaconStruct->HTCaps.supportedChannelWidthSet) &&
                (chanWidthSupp) )
            {
                pAddBssParams->txChannelWidthSet = ( tANI_U8 ) pBeaconStruct->HTInfo.recommendedTxWidthSet;
                pAddBssParams->currentExtChannel = pBeaconStruct->HTInfo.secondaryChannelOffset;
            }
            else
            {
                pAddBssParams->txChannelWidthSet = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
                pAddBssParams->currentExtChannel = PHY_SINGLE_CHANNEL_CENTERED;
            }
            pAddBssParams->llnNonGFCoexist = (tANI_U8)pBeaconStruct->HTInfo.nonGFDevicesPresent;
            pAddBssParams->fLsigTXOPProtectionFullSupport = (tANI_U8)pBeaconStruct->HTInfo.lsigTXOPProtectionFullSupport;
            pAddBssParams->fRIFSMode = pBeaconStruct->HTInfo.rifsMode;
        }
    }

    pAddBssParams->currentOperChannel = bssDescription->channelId;

#ifdef WLAN_FEATURE_11AC
    if (psessionEntry->vhtCapability && ( pBeaconStruct->VHTCaps.present ))
    {
        pAddBssParams->vhtCapable = pBeaconStruct->VHTCaps.present;
        pAddBssParams->vhtTxChannelWidthSet = pBeaconStruct->VHTOperation.chanWidth; 
        pAddBssParams->currentExtChannel = limGet11ACPhyCBState ( pMac,
                                                                  pAddBssParams->currentOperChannel,
                                                                  pAddBssParams->currentExtChannel,
                                                                  psessionEntry->apCenterChan,
                                                                  psessionEntry);
    }
    else 
    {
        pAddBssParams->vhtCapable = 0;
    }
#endif

    // Populate the STA-related parameters here
    // Note that the STA here refers to the AP
    {
        pAddBssParams->staContext.staType = STA_ENTRY_OTHER; // Identifying AP as an STA

        palCopyMemory( pMac->hHdd,  pAddBssParams->staContext.bssId,
                       bssDescription->bssId,
                       sizeof( tSirMacAddr ));
        pAddBssParams->staContext.listenInterval = bssDescription->beaconInterval;

        pAddBssParams->staContext.assocId = 0; // Is SMAC OK with this?
        pAddBssParams->staContext.uAPSD = 0;
        pAddBssParams->staContext.maxSPLen = 0;
        pAddBssParams->staContext.shortPreambleSupported = (tANI_U8)pBeaconStruct->capabilityInfo.shortPreamble;
        pAddBssParams->staContext.updateSta = updateEntry;

        if (IS_DOT11_MODE_HT(psessionEntry->dot11mode) && ( pBeaconStruct->HTCaps.present ))
        {
            pAddBssParams->staContext.us32MaxAmpduDuration = 0;
            pAddBssParams->staContext.htCapable = 1;
            pAddBssParams->staContext.greenFieldCapable  = ( tANI_U8 ) pBeaconStruct->HTCaps.greenField;
            pAddBssParams->staContext.lsigTxopProtection = ( tANI_U8 ) pBeaconStruct->HTCaps.lsigTXOPProtection;
            if( (pBeaconStruct->HTCaps.supportedChannelWidthSet) &&
                (chanWidthSupp) )
            {
                pAddBssParams->staContext.txChannelWidthSet = ( tANI_U8 )pBeaconStruct->HTInfo.recommendedTxWidthSet;
          #ifdef WLAN_FEATURE_11AC
                if (psessionEntry->vhtCapability && ( pBeaconStruct->VHTCaps.present ))
                {
                    pAddBssParams->staContext.vhtCapable = 1;
                    pAddBssParams->staContext.vhtTxChannelWidthSet = pBeaconStruct->VHTOperation.chanWidth; 
                }
          #endif
            }
            else
            {
                pAddBssParams->staContext.txChannelWidthSet = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
            }                                                           
            pAddBssParams->staContext.mimoPS             = (tSirMacHTMIMOPowerSaveState)pBeaconStruct->HTCaps.mimoPowerSave;
            pAddBssParams->staContext.delBASupport       = ( tANI_U8 ) pBeaconStruct->HTCaps.delayedBA;
            pAddBssParams->staContext.maxAmsduSize       = ( tANI_U8 ) pBeaconStruct->HTCaps.maximalAMSDUsize;
            pAddBssParams->staContext.maxAmpduDensity    =             pBeaconStruct->HTCaps.mpduDensity;
            pAddBssParams->staContext.fDsssCckMode40Mhz = (tANI_U8)pBeaconStruct->HTCaps.dsssCckMode40MHz;
            pAddBssParams->staContext.fShortGI20Mhz = (tANI_U8)pBeaconStruct->HTCaps.shortGI20MHz;
            pAddBssParams->staContext.fShortGI40Mhz = (tANI_U8)pBeaconStruct->HTCaps.shortGI40MHz;
            pAddBssParams->staContext.maxAmpduSize= pBeaconStruct->HTCaps.maxRxAMPDUFactor;
            
            if( pBeaconStruct->HTInfo.present )
                pAddBssParams->staContext.rifsMode = pBeaconStruct->HTInfo.rifsMode;
        }

       //If WMM IE or 802.11E IE is not present and AP is HT AP then enable WMM
       if ((psessionEntry->limWmeEnabled && (pBeaconStruct->wmeEdcaPresent || pAddBssParams->staContext.htCapable)) ||
                (psessionEntry->limQosEnabled && (pBeaconStruct->edcaPresent || pAddBssParams->staContext.htCapable)))
            pAddBssParams->staContext.wmmEnabled = 1;
        else 
            pAddBssParams->staContext.wmmEnabled = 0;

        //Update the rates
#ifdef WLAN_FEATURE_11AC
        limPopulateOwnRateSet(pMac, &pAddBssParams->staContext.supportedRates, 
                                        pBeaconStruct->HTCaps.supportedMCSSet, false,psessionEntry,
                                        &pBeaconStruct->VHTCaps);
#else
        limPopulateOwnRateSet(pMac, &pAddBssParams->staContext.supportedRates, 
                                        pBeaconStruct->HTCaps.supportedMCSSet, false,psessionEntry);
#endif
        limFillSupportedRatesInfo(pMac, NULL, &pAddBssParams->staContext.supportedRates,psessionEntry);

    }


    //Disable BA. It will be set as part of ADDBA negotiation.
    for( i = 0; i < STACFG_MAX_TC; i++ )
    {
        pAddBssParams->staContext.staTCParams[i].txUseBA    = eBA_DISABLE;
        pAddBssParams->staContext.staTCParams[i].rxUseBA    = eBA_DISABLE;
        pAddBssParams->staContext.staTCParams[i].txBApolicy = eBA_POLICY_IMMEDIATE;
        pAddBssParams->staContext.staTCParams[i].rxBApolicy = eBA_POLICY_IMMEDIATE;
    }

    pAddBssParams->staContext.encryptType = psessionEntry->encryptType;

#if defined WLAN_FEATURE_VOWIFI  
    pAddBssParams->maxTxPower = psessionEntry->maxTxPower;
#endif

    pAddBssParams->status = eHAL_STATUS_SUCCESS;
    pAddBssParams->respReqd = true;

    pAddBssParams->staContext.sessionId = psessionEntry->peSessionId;
    pAddBssParams->sessionId = psessionEntry->peSessionId;
    
    pAddBssParams->halPersona = (tANI_U8)psessionEntry->pePersona; //update persona

    pAddBssParams->bSpectrumMgtEnabled = psessionEntry->spectrumMgtEnabled || 
        limIsconnectedOnDFSChannel(bssDescription->channelId);

#if defined WLAN_FEATURE_VOWIFI_11R
    pAddBssParams->extSetStaKeyParamValid = 0;
#endif

    // Set a new state for MLME

    //pMac->lim.gLimMlmState = eLIM_MLM_WT_ADD_BSS_RSP_PREASSOC_STATE;
    psessionEntry->limMlmState = eLIM_MLM_WT_ADD_BSS_RSP_PREASSOC_STATE;
    
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));

    //we need to defer the message until we get the response back from HAL.
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
 
    msgQ.type = WDA_ADD_BSS_REQ;
    /** @ToDo : Update the Global counter to keeptrack of the PE <--> HAL messages*/
    msgQ.reserved = 0;
    msgQ.bodyptr = pAddBssParams;
    msgQ.bodyval = 0;

    limLog( pMac, LOG1, FL( "Sending SIR_HAL_ADD_BSS_REQ..." ));
    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));

    retCode = wdaPostCtrlMsg( pMac, &msgQ );
    if( eSIR_SUCCESS != retCode) 
    {
        SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
        palFreeMemory(pMac->hHdd, pAddBssParams);
        limLog( pMac, LOGE, FL("Posting ADD_BSS_REQ to HAL failed, reason=%X\n"),
                retCode );
        goto returnFailure;

    }
    else
    {
        palFreeMemory(pMac->hHdd, pBeaconStruct);    
        return retCode;
    }

 returnFailure:
    // Clean-up will be done by the caller...
    palFreeMemory(pMac->hHdd, pBeaconStruct);
    return retCode;
}





#elif defined(ANI_AP_CLIENT_SDK)
tSirRetStatus limStaSendAddBss( tpAniSirGlobal pMac, tpSirAssocRsp pAssocRsp,
                                     tpSirNeighborBssInfo neighborBssInfo, tANI_U8 updateEntry, 
                                     tpPESession psessionEntry)
{
    tSirMsgQ msgQ;
    tpAddBssParams pAddBssParams = NULL;
    tANI_U32 retCode;
    tANI_U8 i;
    tpDphHashNode pStaDs = NULL;
    
    // Package SIR_HAL_ADD_BSS_REQ message parameters
    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
                                                  (void **) &pAddBssParams,
                                                  sizeof( tAddBssParams )))
    {
        limLog( pMac, LOGP,
                FL( "Unable to PAL allocate memory during ADD_BSS\n" ));
        retCode = eSIR_MEM_ALLOC_FAILED;
        goto returnFailure;
    }
    else
        palZeroMemory( pMac->hHdd, (tANI_U8 *) pAddBssParams, sizeof( tAddBssParams ));

    palCopyMemory( pMac->hHdd,  pAddBssParams->bssId,neighborBssInfo->bssId,
                   sizeof( tSirMacAddr ));

#ifdef WLAN_SOFTAP_FEATURE
    // Fill in tAddBssParams selfMacAddr
    palCopyMemory( pMac->hHdd, pAddBssParams->selfMacAddr, psessionEntry->selfMacAddr,
                  sizeof( tSirMacAddr ));
#endif

    pAddBssParams->bssType = eSIR_INFRASTRUCTURE_MODE;
    pAddBssParams->operMode = BSS_OPERATIONAL_MODE_STA;
    pAddBssParams->beaconInterval = (tANI_U16) neighborBssInfo->beaconInterval;
    pAddBssParams->dtimPeriod = neighborBssInfo->dtimPeriod;
    pAddBssParams->updateBss = updateEntry;

   
    /* The following parameters are commented since the information is not available from the 
     * neighborBssInfo. This needs to be fixed later */
#if 0    

    pAddBssParams->cfParamSet.cfpCount = beaconStruct.cfParamSet.cfpCount;
    pAddBssParams->cfParamSet.cfpPeriod = beaconStruct.cfParamSet.cfpPeriod;
    pAddBssParams->cfParamSet.cfpMaxDuration = beaconStruct.cfParamSet.cfpMaxDuration;
    pAddBssParams->cfParamSet.cfpDurRemaining = beaconStruct.cfParamSet.cfpDurRemaining;
#endif
    pAddBssParams->rateSet.numRates = pAssocRsp->supportedRates.numRates;
    palCopyMemory( pMac->hHdd,  pAddBssParams->rateSet.rate,
                   pAssocRsp->supportedRates.rate, pAssocRsp->supportedRates.numRates );

     /* Update PE session ID */
    pAddBssParams->sessionId = psessionEntry->peSessionId;

    pAddBssParams->nwType = neighborBssInfo->nwType;
    
    pAddBssParams->shortSlotTimeSupported = (tANI_U8)pAssocRsp->capabilityInfo.shortSlotTime;    
    pAddBssParams->llaCoexist = (tANI_U8) psessionEntry->beaconParams.llaCoexist;    
    pAddBssParams->llbCoexist = (tANI_U8) psessionEntry->beaconParams.llbCoexist;
    pAddBssParams->llgCoexist = (tANI_U8) psessionEntry->beaconParams.llgCoexist;
    pAddBssParams->ht20Coexist = (tANI_U8) psessionEntry->beaconParams.ht20Coexist;    

    // Use the advertised capabilities from the received beacon/PR
    if (IS_DOT11_MODE_HT(psessionEntry->dot11mode) && ( neighborBssInfo->HTCapsPresent ))
    {
        pAddBssParams->htCapable = pAssocRsp->HTCaps.present;

        if ( neighborBssInfo->HTInfoPresent )
        {
            pAddBssParams->htOperMode = pAssocRsp->HTInfo.opMode;
            pAddBssParams->dualCTSProtection = ( tANI_U8 )pAssocRsp->HTInfo.dualCTSProtection;
 
            if(pAssocRsp->HTCaps.supportedChannelWidthSet)
            {
                pAddBssParams->txChannelWidthSet = ( tANI_U8 )pAssocRsp->HTInfo.recommendedTxWidthSet;
                pAddBssParams->currentExtChannel = pAssocRsp->HTInfo.secondaryChannelOffset;
            }
            else
            {
                pAddBssParams->txChannelWidthSet = (tANI_U8)pAssocRsp->HTCaps.supportedChannelWidthSet;
                pAddBssParams->currentExtChannel = PHY_SINGLE_CHANNEL_CENTERED;
            }
            pAddBssParams->llnNonGFCoexist = (tANI_U8)pAssocRsp->HTInfo.nonGFDevicesPresent;
            pAddBssParams->fLsigTXOPProtectionFullSupport = (tANI_U8)pAssocRsp->HTInfo.lsigTXOPProtectionFullSupport;
            pAddBssParams->fRIFSMode = pAssocRsp->HTInfo.rifsMode;
        }
    }

    pAddBssParams->currentOperChannel = neighborBssInfo->channelId;

    // Populate the STA-related parameters here
    // Note that the STA here refers to the AP
    {
        pAddBssParams->staContext.staType = STA_ENTRY_OTHER; // Identifying AP as an STA

        palCopyMemory( pMac->hHdd,  pAddBssParams->staContext.bssId,
                       neighborBssInfo->bssId,
                       sizeof( tSirMacAddr ));
        pAddBssParams->staContext.listenInterval = (tANI_U8) neighborBssInfo->beaconInterval;

        pAddBssParams->staContext.assocId = 0; // Is SMAC OK with this?
        pAddBssParams->staContext.uAPSD = 0;
        pAddBssParams->staContext.maxSPLen = 0;
        pAddBssParams->staContext.shortPreambleSupported = (tANI_U8)pAssocRsp->capabilityInfo.shortPreamble;
        pAddBssParams->staContext.updateSta = updateEntry;


        if (IS_DOT11_MODE_HT(psessionEntry->dot11mode) && ( pAssocRsp->HTCaps.present ))
        {
            pAddBssParams->staContext.us32MaxAmpduDuration = 0;
            pAddBssParams->staContext.htCapable = 1;
            pAddBssParams->staContext.greenFieldCapable  = ( tANI_U8 )pAssocRsp->HTCaps.greenField;
            pAddBssParams->staContext.lsigTxopProtection = ( tANI_U8 )pAssocRsp->HTCaps.lsigTXOPProtection;
            pAddBssParams->staContext.txChannelWidthSet  = ( tANI_U8 )(pAssocRsp->HTCaps.supportedChannelWidthSet ?
                                                                                         pAssocRsp->HTInfo.recommendedTxWidthSet : 
                                                                                         pAssocRsp->HTCaps.supportedChannelWidthSet );
            pAddBssParams->staContext.mimoPS             =             pAssocRsp->HTCaps.mimoPowerSave;
            pAddBssParams->staContext.delBASupport       = ( tANI_U8 )pAssocRsp->HTCaps.delayedBA;
            pAddBssParams->staContext.maxAmsduSize       = ( tANI_U8 )pAssocRsp->HTCaps.maximalAMSDUsize;
            pAddBssParams->staContext.maxAmpduDensity    =             pAssocRsp->HTCaps.mpduDensity;
            pAddBssParams->staContext.fDsssCckMode40Mhz = (tANI_U8)pAssocRsp->HTCaps.dsssCckMode40MHz;
            pAddBssParams->staContext.fShortGI20Mhz = (tANI_U8)pAssocRsp->HTCaps.shortGI20MHz;
            pAddBssParams->staContext.fShortGI40Mhz = (tANI_U8)pAssocRsp->HTCaps.shortGI40MHz;
            pAddBssParams->staContext.maxAmpduSize= pAssocRsp->HTCaps.maxRxAMPDUFactor;
            
            if( pAssocRsp->HTInfo.present )
                pAddBssParams->staContext.rifsMode = pAssocRsp->HTInfo.rifsMode;
        }

        if ((psessionEntry->limWmeEnabled && pAssocRsp->wmeEdcaPresent) ||
                (psessionEntry->limQosEnabled && pAssocRsp->edcaPresent))
            pAddBssParams->staContext.wmmEnabled = 1;
        else 
            pAddBssParams->staContext.wmmEnabled = 0;

        //Update the rates

        pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);
        if (pStaDs != NULL)
        {
            limFillSupportedRatesInfo(pMac, pStaDs, &pStaDs->supportedRates,psessionEntry);
            palCopyMemory(pMac->hHdd, (tANI_U8*)&pAddBssParams->staContext.supportedRates,
                                                (tANI_U8*)&pStaDs->supportedRates,
                                                sizeof(tSirSupportedRates));
        }
        else
            PELOGE(limLog(pMac, LOGE, FL("could not Update the supported rates.\n"));)

    }

    //Disable BA. It will be set as part of ADDBA negotiation.
    for( i = 0; i < SMAC_STACFG_MAX_TC; i++ )
    {
        pAddBssParams->staContext.staTCParams[i].txUseBA    = eBA_DISABLE;
        pAddBssParams->staContext.staTCParams[i].rxUseBA    = eBA_DISABLE;
        pAddBssParams->staContext.staTCParams[i].txBApolicy = eBA_POLICY_IMMEDIATE;
        pAddBssParams->staContext.staTCParams[i].rxBApolicy = eBA_POLICY_IMMEDIATE;
    }

#if defined WLAN_FEATURE_VOWIFI  
    pAddBssParams->maxTxPower = psessionEntry->maxTxPower;
#endif
    // FIXME_GEN4 - Any other value that can be used for initialization?
    pAddBssParams->status = eHAL_STATUS_SUCCESS;
    pAddBssParams->respReqd = true;

    pAddBssParams->halPersona = (tANI_U8)psessionEntry->pePersona; //update persona

#if defined WLAN_FEATURE_VOWIFI_11R
    pAddBssParams->extSetStaKeyParamValid = 0;
#endif

    // Set a new state for MLME
    if( eLIM_MLM_WT_ASSOC_RSP_STATE == psessionEntry->limMlmState )
        psessionEntry->limMlmState = eLIM_MLM_WT_ADD_BSS_RSP_ASSOC_STATE;
    else
        psessionEntry->limMlmState = eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));

    //we need to defer the message until we get the response back from HAL.
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);

    msgQ.type = SIR_HAL_ADD_BSS_REQ;
    /** @ToDo : Update the Global counter to keeptrack of the PE <--> HAL messages*/
    msgQ.reserved = 0;
    msgQ.bodyptr = pAddBssParams;
    msgQ.bodyval = 0;

    limLog( pMac, LOG1, FL( "Sending SIR_HAL_ADD_BSS_REQ..." ));
    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));

    retCode = halPostMsgApi( pMac, &msgQ );
    if( eSIR_SUCCESS != retCode) 
    {
        palFreeMemory(pMac->hHdd, pAddBssParams);
        limLog( pMac, LOGE, FL("Posting ADD_BSS_REQ to HAL failed, reason=%X\n"),
                retCode );
    }
    else
        return retCode;

 returnFailure:
    // Clean-up will be done by the caller...
    return retCode;
}

#endif // ANI_PRODUCT_TYPE_CLIENT

/** -------------------------------------------------------------
\fn limPrepareAndSendDelStaCnf
\brief deletes DPH entry
                    changes the MLM mode for station.
                    calls limSendDelStaCnf
\param     tpAniSirGlobal    pMac
\param         tpDphHashNode pStaDs
\return none
  -------------------------------------------------------------*/


void
limPrepareAndSendDelStaCnf(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tSirResultCodes statusCode,tpPESession psessionEntry)
{
    tANI_U16 staDsAssocId = 0;
    tSirMacAddr staDsAddr;
    tLimMlmStaContext mlmStaContext;

    if(pStaDs == NULL)
    {
      PELOGW(limLog(pMac, LOGW, FL("pStaDs is NULL\n"));)
      return;
    }
    staDsAssocId = pStaDs->assocId;
    palCopyMemory( pMac->hHdd, (tANI_U8 *)staDsAddr,
            pStaDs->staAddr,
            sizeof(tSirMacAddr));

    mlmStaContext = pStaDs->mlmStaContext;
    if(eSIR_SME_SUCCESS == statusCode)
    {
        if ((psessionEntry->limSystemRole == eLIM_AP_ROLE) ||
            (psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE))
        {
            limReleasePeerIdx(pMac, pStaDs->assocId, psessionEntry);
        }

        limDeleteDphHashEntry(pMac, pStaDs->staAddr, pStaDs->assocId,psessionEntry);
    }
    if ( (psessionEntry->limSystemRole == eLIM_STA_ROLE)||(psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE))
    {
        psessionEntry->limMlmState = eLIM_MLM_IDLE_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));
    }

    limSendDelStaCnf(pMac, staDsAddr, staDsAssocId, mlmStaContext, statusCode,psessionEntry);
}

/** -------------------------------------------------------------
\fn limGetStaRateMode
\brief Gets the Station Rate Mode.
\param     tANI_U8 dot11Mode
\return none
  -------------------------------------------------------------*/
tStaRateMode limGetStaRateMode(tANI_U8 dot11Mode)
{
    switch(dot11Mode)
        {
            case WNI_CFG_DOT11_MODE_11A:
                return eSTA_11a;
            case WNI_CFG_DOT11_MODE_11B:
                return eSTA_11b;
            case WNI_CFG_DOT11_MODE_11G:
                return eSTA_11bg;
            case WNI_CFG_DOT11_MODE_11N:
                return eSTA_11n;
#ifdef WLAN_FEATURE_11AC
            case WNI_CFG_DOT11_MODE_11AC:
                return eSTA_11ac;
#endif
            case WNI_CFG_DOT11_MODE_ALL:
            default:
                return eSTA_11n;           
            
        }
}

/** -------------------------------------------------------------
\fn limInitPreAuthTimerTable
\brief Initialize the Pre Auth Tanle and creates the timer for 
       each node for the timeout value got from cfg.
\param     tpAniSirGlobal    pMac
\param     tpLimPreAuthTable pPreAuthTimerTable
\return none
  -------------------------------------------------------------*/
void limInitPreAuthTimerTable(tpAniSirGlobal pMac, tpLimPreAuthTable pPreAuthTimerTable)
{
    tANI_U32 cfgValue;
    tANI_U32 authNodeIdx;
    tpLimPreAuthNode pAuthNode = pPreAuthTimerTable->pTable;
    
    // Get AUTH_RSP Timers value

    if (wlan_cfgGetInt(pMac, WNI_CFG_AUTHENTICATE_RSP_TIMEOUT,
                 &cfgValue) != eSIR_SUCCESS)
    {
        /*
        ** Could not get AUTH_RSP timeout value
        ** from CFG. Log error.
        **/
        limLog(pMac, LOGP,
               FL("could not retrieve AUTH_RSP timeout value\n"));
        return;
    }

    cfgValue = SYS_MS_TO_TICKS(cfgValue);
    for(authNodeIdx=0; authNodeIdx<pPreAuthTimerTable->numEntry; authNodeIdx++, pAuthNode++)
    {
        if (tx_timer_create(&pAuthNode->timer,
                        "AUTH RESPONSE TIMEOUT",
                        limAuthResponseTimerHandler,
                        authNodeIdx,
                        cfgValue,
                        0,
                        TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            // Cannot create timer.  Log error.
            limLog(pMac, LOGP, FL("Cannot create Auth Rsp timer of Index :%d.\n"), authNodeIdx);
            return;
        }
        pAuthNode->authNodeIdx = (tANI_U8)authNodeIdx;
        pAuthNode->fFree = 1;
    }

}

/** -------------------------------------------------------------
\fn limAcquireFreePreAuthNode
\brief Retrives a free Pre Auth node from Pre Auth Table.
\param     tpAniSirGlobal    pMac
\param     tpLimPreAuthTable pPreAuthTimerTable
\return none
  -------------------------------------------------------------*/
tLimPreAuthNode * limAcquireFreePreAuthNode(tpAniSirGlobal pMac, tpLimPreAuthTable pPreAuthTimerTable)
{
    tANI_U32 i;
    tLimPreAuthNode *pTempNode = pPreAuthTimerTable->pTable;
    for (i=0; i<pPreAuthTimerTable->numEntry; i++,pTempNode++)
    {
        if (pTempNode->fFree == 1)
        {
            pTempNode->fFree = 0;
            return pTempNode;
        }
    }

    return NULL;
}

/** -------------------------------------------------------------
\fn limGetPreAuthNodeFromIndex
\brief Depending on the Index this retrives the pre auth node.
\param     tpAniSirGlobal    pMac
\param     tpLimPreAuthTable pAuthTable
\param     tANI_U32 authNodeIdx
\return none
  -------------------------------------------------------------*/
tLimPreAuthNode * limGetPreAuthNodeFromIndex(tpAniSirGlobal pMac, 
                               tpLimPreAuthTable pAuthTable, tANI_U32 authNodeIdx)
{
    if ((authNodeIdx >= pAuthTable->numEntry) || (pAuthTable->pTable == NULL))
    {
        limLog(pMac, LOGE, FL("Invalid Auth Timer Index : %d NumEntry : %d\n"), 
                  authNodeIdx, pAuthTable->numEntry);
        return NULL;
    }
    
    return pAuthTable->pTable + authNodeIdx;
}

/* Util API to check if the channels supported by STA is within range */
tSirRetStatus limIsDot11hSupportedChannelsValid(tpAniSirGlobal pMac, tSirAssocReq *assoc)
{
    /*
         * Allow all the stations to join with us.
         * 802.11h-2003 11.6.1 => An AP may use the supported channels list for associated STAs
         * as an input into an algorithm used to select a new channel for the BSS.
         * The specification of the algorithm is beyond the scope of this amendment.
         */

    return (eSIR_SUCCESS);  
}

/* Util API to check if the txpower supported by STA is within range */
tSirRetStatus limIsDot11hPowerCapabilitiesInRange(tpAniSirGlobal pMac, tSirAssocReq *assoc,tpPESession psessionEntry)
{
    tPowerdBm localMaxTxPower;
    tANI_U32 localPwrConstraint; 

    localMaxTxPower = cfgGetRegulatoryMaxTransmitPower(pMac, psessionEntry->currentOperChannel);

    if(wlan_cfgGetInt(pMac, WNI_CFG_LOCAL_POWER_CONSTRAINT, &localPwrConstraint) != eSIR_SUCCESS) {
        limLog( pMac, LOGP, FL( "Unable to get Local Power Constraint from cfg\n" ));
        return eSIR_FAILURE;
    }
    localMaxTxPower -= (tPowerdBm)localPwrConstraint;

    /**
         *  The min Tx Power of the associating station should not be greater than (regulatory
         *  max tx power - local power constraint configured on AP).
         */
    if(assoc->powerCapability.minTxPower > localMaxTxPower)
    {
        limLog(pMac, LOGW, FL("minTxPower (STA) = %d, localMaxTxPower (AP) = %d\n"),
                       assoc->powerCapability.minTxPower, localMaxTxPower);
        return (eSIR_FAILURE);
    }
    
    return (eSIR_SUCCESS);
}

/** -------------------------------------------------------------
\fn     limFillRxHighestSupportedRate
\brief  Fills in the Rx Highest Supported Data Rate field from 
\       the 'supported MCS set' field in HT capability element. 
\param  tpAniSirGlobal    pMac
\param  tpSirSupportedRates  pRates
\param  tANI_U8*  pSupportedMCSSet
\return none
  -------------------------------------------------------------*/
void limFillRxHighestSupportedRate(tpAniSirGlobal pMac, tANI_U16 *rxHighestRate, tANI_U8* pSupportedMCSSet)
{
    tSirMacRxHighestSupportRate  *pRxHighestRate;
    tANI_U8  *pBuf; 
    tANI_U16  rate=0;

    pBuf = pSupportedMCSSet + MCS_RX_HIGHEST_SUPPORTED_RATE_BYTE_OFFSET;
    rate = limGetU16(pBuf);

    pRxHighestRate = (tSirMacRxHighestSupportRate *) &rate;
    *rxHighestRate = pRxHighestRate->rate;

    return;
}
