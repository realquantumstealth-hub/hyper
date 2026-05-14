# Project Structure Overview

## Directory Layout

### `/src` - Source Code
All source code is organized under this directory:

#### `/src/bootloader`
- **Purpose:** UEFI bootloader component
- **Output:** `bootloader.efi`
- **Language:** C
- **Key Files:**
  - `src/main.c` - Entry point
  - `src/bootmgfw/` - Boot manager functionality
  - `src/hooks/` - UEFI hooks
  - `src/hvloader/` - Hypervisor loader
  - `src/tpm/` - TPM-related components

#### `/src/hypervisor`
- **Purpose:** Hypervisor kernel driver
- **Output:** `hypervisor.dll`
- **Language:** C++ (C++20)
- **Key Files:**
  - `src/main.cpp` - Entry point
  - `src/arch/` - Architecture-specific code
  - `src/slat/` - SLAT implementation
  - `src/interrupts/` - Interrupt handling
  - `src/memory_manager/` - Memory management
  - `src/hypercall/` - Hypercall interface

#### `/src/client`
- **Purpose:** Usermode client application
- **Output:** `client.exe`
- **Language:** C++ (Latest standard)
- **Key Files:**
  - `src/main.cpp` - Entry point
  - `src/commands/` - Command processing
  - `src/hook/` - Hooking functionality
  - `src/dll_loader/` - DLL injection
  - `src/hypercall/` - Hypercall interface

#### `/src/common`
- **Purpose:** Shared code and structures
- **Contents:**
  - `hypercall/` - Hypercall definitions
  - `structures/` - Common data structures

### `/tests` - Test Projects
Contains test and example projects:
- `/tests/basic-test` - Basic functionality test

### `/external` - External Dependencies
Third-party libraries and dependencies:
- Future location for consolidated external dependencies

### `/docs` - Documentation
Project documentation:
- Architecture documents
- Build guides
- Component overviews

### `/build` - Build Output
Compiled binaries and build artifacts:
- `/build/x64/Release/` - Release builds
- `/build/x64/Release/obj/` - Object files

## Component Relationships

```
┌─────────────────┐
│   bootloader    │
│   (UEFI Boot)   │
└────────┬────────┘
         │ Loads
         ▼
┌─────────────────┐
│   hypervisor    │◄────┐
│  (Kernel Mode)  │     │
└────────┬────────┘     │
         │              │ Hypercalls
         │ Manages      │
         ▼              │
┌─────────────────┐     │
│   Windows OS    │     │
└─────────────────┘     │
         ▲              │
         │ Controls     │
         │              │
┌────────┴────────┐     │
│     client      ├─────┘
│  (User Mode)    │
└─────────────────┘
```

## File Naming Conventions

### Project Files
- `bootloader.vcxproj` - Bootloader Visual Studio project
- `hypervisor.vcxproj` - Hypervisor Visual Studio project
- `client.vcxproj` - Client Visual Studio project
- `basic-test.vcxproj` - Test project

### Solution File
- `hyper.sln` - Main Visual Studio solution

## Build Configuration

### Output Locations
All projects build to: `$(SolutionDir)\build\$(Platform)\$(Configuration)\`

### Include Paths
- Common code: `$(SolutionDir)src\common`
- External libraries: `$(ProjectDir)ext`

### Intermediate Files
Object files: `$(SolutionDir)\build\$(Platform)\$(Configuration)\obj\$(TargetName)\`

## Migration Notes