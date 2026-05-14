#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseCryptLib.h>
#include <IndustryStandard/PeImage.h>

// Global variables for hash storage
UINT8 LegitimateBootmgfwHash[32] = {0};
BOOLEAN HashComputed = FALSE;

// Function to compute Authenticode PE/COFF hash of an image
EFI_STATUS
ComputeAuthenticodeHash(
    IN  UINT8   *ImageBase,
    IN  UINTN   ImageSize,
    OUT UINT8   *Hash
    )
{
    EFI_STATUS                    Status;
    EFI_IMAGE_DOS_HEADER          *DosHdr;
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION NtHeader;
    UINT32                        PeCoffHeaderOffset;
    UINT8                         *HashBase;
    UINTN                         HashSize;
    UINTN                         SumOfBytesHashed;
    EFI_IMAGE_SECTION_HEADER      *Section;
    EFI_IMAGE_SECTION_HEADER      *SectionHeader;
    UINTN                         Index;
    UINTN                         Pos;
    UINT32                        CertSize;
    UINT32                        NumberOfRvaAndSizes;
    VOID                          *HashCtx;
    BOOLEAN                       Result;

    if (ImageBase == NULL || Hash == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Check DOS header
    DosHdr = (EFI_IMAGE_DOS_HEADER *)ImageBase;
    if (DosHdr->e_magic != EFI_IMAGE_DOS_SIGNATURE) {
        Print(L"[!] Invalid DOS signature\n");
        return EFI_INVALID_PARAMETER;
    }

    PeCoffHeaderOffset = DosHdr->e_lfanew;
    
    // Check PE header
    NtHeader.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)(ImageBase + PeCoffHeaderOffset);
    if (NtHeader.Pe32->Signature != EFI_IMAGE_NT_SIGNATURE) {
        Print(L"[!] Invalid PE signature\n");
        return EFI_INVALID_PARAMETER;
    }

    // Allocate hash context
    HashCtx = AllocatePool(Sha256GetContextSize());
    if (HashCtx == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    // Initialize SHA256 context
    Result = Sha256Init(HashCtx);
    if (!Result) {
        FreePool(HashCtx);
        return EFI_ABORTED;
    }

    //
    // Authenticode Hash Calculation:
    // 1. Hash everything from file start to beginning of checksum
    //
    HashBase = ImageBase;
    if (NtHeader.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        // PE32
        HashSize = (UINTN)(&NtHeader.Pe32->OptionalHeader.CheckSum) - (UINTN)HashBase;
        NumberOfRvaAndSizes = NtHeader.Pe32->OptionalHeader.NumberOfRvaAndSizes;
    } else {
        // PE32+
        HashSize = (UINTN)(&NtHeader.Pe32Plus->OptionalHeader.CheckSum) - (UINTN)HashBase;
        NumberOfRvaAndSizes = NtHeader.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes;
    }

    Result = Sha256Update(HashCtx, HashBase, HashSize);
    if (!Result) {
        FreePool(HashCtx);
        return EFI_ABORTED;
    }

    //
    // 2. Skip checksum (4 bytes)
    // 3. Hash from after checksum to Security Directory
    //
    if (NumberOfRvaAndSizes <= EFI_IMAGE_DIRECTORY_ENTRY_SECURITY) {
        // No security directory
        if (NtHeader.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            HashBase = (UINT8 *)&NtHeader.Pe32->OptionalHeader.CheckSum + sizeof(UINT32);
            HashSize = NtHeader.Pe32->OptionalHeader.SizeOfHeaders - ((UINTN)HashBase - (UINTN)ImageBase);
        } else {
            HashBase = (UINT8 *)&NtHeader.Pe32Plus->OptionalHeader.CheckSum + sizeof(UINT32);
            HashSize = NtHeader.Pe32Plus->OptionalHeader.SizeOfHeaders - ((UINTN)HashBase - (UINTN)ImageBase);
        }
    } else {
        // Has security directory - hash up to it
        if (NtHeader.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            HashBase = (UINT8 *)&NtHeader.Pe32->OptionalHeader.CheckSum + sizeof(UINT32);
            HashSize = (UINTN)(&NtHeader.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY]) - (UINTN)HashBase;
        } else {
            HashBase = (UINT8 *)&NtHeader.Pe32Plus->OptionalHeader.CheckSum + sizeof(UINT32);
            HashSize = (UINTN)(&NtHeader.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY]) - (UINTN)HashBase;
        }
    }

    if (HashSize != 0) {
        Result = Sha256Update(HashCtx, HashBase, HashSize);
        if (!Result) {
            FreePool(HashCtx);
            return EFI_ABORTED;
        }
    }

    //
    // 4. Skip Security Directory (8 bytes) if present
    // 5. Hash everything after Security Directory in headers
    //
    if (NumberOfRvaAndSizes > EFI_IMAGE_DIRECTORY_ENTRY_SECURITY) {
        if (NtHeader.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            HashBase = (UINT8 *)&NtHeader.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY + 1];
            HashSize = NtHeader.Pe32->OptionalHeader.SizeOfHeaders - ((UINTN)HashBase - (UINTN)ImageBase);
        } else {
            HashBase = (UINT8 *)&NtHeader.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY + 1];
            HashSize = NtHeader.Pe32Plus->OptionalHeader.SizeOfHeaders - ((UINTN)HashBase - (UINTN)ImageBase);
        }

        if (HashSize != 0) {
            Result = Sha256Update(HashCtx, HashBase, HashSize);
            if (!Result) {
                FreePool(HashCtx);
                return EFI_ABORTED;
            }
        }
    }

    //
    // 6. Set SumOfBytesHashed to SizeOfHeaders
    //
    if (NtHeader.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        SumOfBytesHashed = NtHeader.Pe32->OptionalHeader.SizeOfHeaders;
    } else {
        SumOfBytesHashed = NtHeader.Pe32Plus->OptionalHeader.SizeOfHeaders;
    }

    //
    // 7. Build Section Table
    //
    Section = (EFI_IMAGE_SECTION_HEADER *)(
                 ImageBase +
                 PeCoffHeaderOffset +
                 sizeof(UINT32) +
                 sizeof(EFI_IMAGE_FILE_HEADER) +
                 NtHeader.Pe32->FileHeader.SizeOfOptionalHeader
                 );

    SectionHeader = AllocateZeroPool(sizeof(EFI_IMAGE_SECTION_HEADER) * NtHeader.Pe32->FileHeader.NumberOfSections);
    if (SectionHeader == NULL) {
        FreePool(HashCtx);
        return EFI_OUT_OF_RESOURCES;
    }

    //
    // 8. Sort sections by PointerToRawData
    //
    for (Index = 0; Index < NtHeader.Pe32->FileHeader.NumberOfSections; Index++) {
        Pos = Index;
        while ((Pos > 0) && (Section->PointerToRawData < SectionHeader[Pos - 1].PointerToRawData)) {
            CopyMem(&SectionHeader[Pos], &SectionHeader[Pos - 1], sizeof(EFI_IMAGE_SECTION_HEADER));
            Pos--;
        }
        CopyMem(&SectionHeader[Pos], Section, sizeof(EFI_IMAGE_SECTION_HEADER));
        Section++;
    }

    //
    // 9. Hash all sections
    //
    for (Index = 0; Index < NtHeader.Pe32->FileHeader.NumberOfSections; Index++) {
        Section = &SectionHeader[Index];
        if (Section->SizeOfRawData == 0) {
            continue;
        }
        HashBase = ImageBase + Section->PointerToRawData;
        HashSize = (UINTN)Section->SizeOfRawData;

        Result = Sha256Update(HashCtx, HashBase, HashSize);
        if (!Result) {
            FreePool(SectionHeader);
            FreePool(HashCtx);
            return EFI_ABORTED;
        }

        SumOfBytesHashed += HashSize;
    }

    //
    // 10. Hash any trailing data (excluding Authenticode signature)
    //
    if (ImageSize > SumOfBytesHashed) {
        HashBase = ImageBase + SumOfBytesHashed;

        // Get the security directory location and size
        UINT32 SecurityDirRVA = 0;
        UINT32 SecurityDirSize = 0;
        
        if (NumberOfRvaAndSizes > EFI_IMAGE_DIRECTORY_ENTRY_SECURITY) {
            if (NtHeader.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
                SecurityDirRVA = NtHeader.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress;
                SecurityDirSize = NtHeader.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size;
            } else {
                SecurityDirRVA = NtHeader.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress;
                SecurityDirSize = NtHeader.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size;
            }
        }

        // If there's a security directory, exclude it from the hash
        if (SecurityDirRVA != 0 && SecurityDirSize != 0) {
            // The security directory RVA is actually a file offset for the signature
            UINT32 SecurityDirOffset = SecurityDirRVA;
            
            // Only hash up to the security directory
            if (SecurityDirOffset > SumOfBytesHashed && SecurityDirOffset < ImageSize) {
                HashSize = (UINTN)(SecurityDirOffset - SumOfBytesHashed);
                
                if (HashSize > 0) {
                    Result = Sha256Update(HashCtx, HashBase, HashSize);
                    if (!Result) {
                        FreePool(SectionHeader);
                        FreePool(HashCtx);
                        return EFI_ABORTED;
                    }
                }
            }
        } else {
            // No security directory, hash everything to the end
            HashSize = (UINTN)(ImageSize - SumOfBytesHashed);
            
            if (HashSize > 0) {
                Result = Sha256Update(HashCtx, HashBase, HashSize);
                if (!Result) {
                    FreePool(SectionHeader);
                    FreePool(HashCtx);
                    return EFI_ABORTED;
                }
            }
        }
    }

    // Finalize hash
    Result = Sha256Final(HashCtx, Hash);
    
    FreePool(SectionHeader);
    FreePool(HashCtx);

    if (!Result) {
        return EFI_ABORTED;
    }

    // Add detailed debug information
    Print(L"[*] Hash computation details:\n");
    Print(L"  - File size: %lu bytes\n", ImageSize);
    Print(L"  - PE type: %s\n", (NtHeader.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) ? L"PE32" : L"PE32+");
    Print(L"  - Number of sections: %u\n", NtHeader.Pe32->FileHeader.NumberOfSections);
    Print(L"  - Size of headers: %u\n", (NtHeader.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) ? 
                                          NtHeader.Pe32->OptionalHeader.SizeOfHeaders : 
                                          NtHeader.Pe32Plus->OptionalHeader.SizeOfHeaders);
    
    if (NumberOfRvaAndSizes > EFI_IMAGE_DIRECTORY_ENTRY_SECURITY) {
        UINT32 SecurityDirRVA, SecurityDirSize;
        if (NtHeader.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            SecurityDirRVA = NtHeader.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress;
            SecurityDirSize = NtHeader.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size;
        } else {
            SecurityDirRVA = NtHeader.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress;
            SecurityDirSize = NtHeader.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size;
        }
        Print(L"  - Security Dir RVA: 0x%08x\n", SecurityDirRVA);
        Print(L"  - Security Dir Size: %u\n", SecurityDirSize);
    }
    
    Print(L"[*] Computed Authenticode SHA256 hash: ");
    for (Index = 0; Index < 32; Index++) {
        Print(L"%02x", Hash[Index]);
    }
    Print(L"\n");

    return EFI_SUCCESS;
}

// Function to compute and store the legitimate bootmgfw hash
EFI_STATUS
ComputeAndStoreLegitimateBootmgfwHash(
    IN  UINT8   *ImageBase,
    IN  UINTN   ImageSize
    )
{
    EFI_STATUS Status;
    
    if (HashComputed) {
        Print(L"[*] Hash already computed, skipping...\n");
        return EFI_SUCCESS;
    }
    
    Status = ComputeAuthenticodeHash(ImageBase, ImageSize, LegitimateBootmgfwHash);
    if (EFI_ERROR(Status)) {
        Print(L"[!] Failed to compute legitimate bootmgfw hash: %r\n", Status);
        return Status;
    }
    
    HashComputed = TRUE;
    Print(L"[*] Legitimate bootmgfw hash computed and stored successfully\n");
    
    return EFI_SUCCESS;
}
