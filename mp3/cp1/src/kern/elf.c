#include "elf.h"
#include "heap.h"
#include "string.h"
#include "error.h"
#include "console.h"  // For kprintf, used to output debugging information
#include "io.h"

#define PT_LOAD 1  // Segment type for loadable segments
#define START_ADDR 0x80100000
#define END_ADDR 0x81000000
#define EI_NIDENT 16

#define EI_MAG0_IDX 0
#define EI_MAG1_IDX 1
#define EI_MAG2_IDX 2
#define EI_MAG3_IDX 3
#define EI_CLASS_IDX 4
#define EI_DATA_IDX 5
#define EI_VERSION_IDX 6
#define EI_OSABI_IDX 7
#define EI_ABIVERSION_IDX 8
#define EI_PAD_IDX 9

// ELF magic number
#define EI_MAG0 0x7f
#define EI_MAG1 'E'
#define EI_MAG2 'L'
#define EI_MAG3 'F'
// ELF class
#define ELFCLASSNONE 0  // Invalid class
#define ELFCLASS32 1    // 32-bit objects
#define ELFCLASS64 2    // 64-bit objects
// ELF data encoding
#define ELFDATANONE 0  // Invalid data encoding
#define ELFDATA2LSB 1  // Little-endian data encoding
#define ELFDATA2MSB 2  // Big-endian data encoding
// EI version
#define EI_NONE 0  // Invalid version
#define EI_CURRENT 1  // Current version
// ELF OSABI
#define ELFOSABI_NONE 0  // No extensions or unspecified
#define ELFOSABI_SYSV 1  // UNIX System V ABI

// e_type values
#define ET_EXEC 2  // Executable file

// e_machine values
#define EM_RISCV64 0xf3  // RISC-V ISA

// e_version values
#define EV_CURRENT 1  // Current version
// Structure representing the ELF file header for a 32-bit ELF file
struct Elf64_Ehdr {
    uint8_t e_ident[EI_NIDENT];    // ELF magic number - 16 bit long
    uint16_t e_type;        // Type of ELF file
    uint16_t e_machine;     // Target architecture - riscv64
    uint32_t e_version;     // ELF version (usually 1 for the original ELF)
    uint64_t e_entry;       // Entry point address where execution starts
    uint64_t e_phoff;       // Offset to the program header table
    uint64_t e_shoff;       // Offset to the section header table
    uint32_t e_flags;       // Processor-specific flags
    uint16_t e_ehsize;      // Size of this ELF header
    uint16_t e_phentsize;   // Size of each program header table entry
    uint16_t e_phnum;       // Number of entries in the program header table
    uint16_t e_shentsize;   // Size of each section header table entry
    uint16_t e_shnum;       // Number of entries in the section header table
    uint16_t e_shstrndx;    // Section header table index for the section name string table
}__attribute__((packed));

// Structure representing a program header in the ELF file
struct Elf64_Phdr {
    uint32_t p_type;    // Type of segment
    uint32_t p_flags;   // Segment flags
    uint64_t p_offset;  // Offset of the segment in the ELF file
    uint64_t p_vaddr;   // Virtual address of the segment in memory
    uint64_t p_paddr;   // Physical address of the segment in memory
    uint64_t p_filesz;  // Size of the segment in the ELF file
    uint64_t p_memsz;   // Size of the segment in memory
    uint64_t p_align;   // Alignment of the segment
}__attribute__((packed));

// Function to load an ELF executable file into memory for 64-bit architecture


/*
* function header for int elf_load(struct io_intf *io, void (**entryptr)(struct io_intf *io))
*
* inputs:
*    - struct io_intf *io: pointer to the I/O interface used to read the ELF file
*    - void (**entryptr)(struct io_intf *io): pointer to a function pointer where the ELF entry
*      point address will be stored, allowing execution of the loaded program
*
* outputs:
*    - int: returns 0 on success, or an error code on failure:
*
* effects:
*    - Loads an ELF executable file into memory, verifies its structure and attributes
*    - Sets `entryptr` to the entry point of the loaded ELF file for execution
*
* description:
*    - Checks the ELF header for validity (magic number, class, endianness, etc.)
*    - Iterates through each program header entry, loading segments of type `PT_LOAD`
*    - If any criteria are unmet, it returns the appropriate error code
*/

int elf_load(struct io_intf *io, void (**entryptr)(struct io_intf *io)) {
    struct Elf64_Ehdr ehdr;

    // Read the ELF file header from the I/O interface
    if (ioread_full(io, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        kprintf("Failed to read ELF header\n");
        return -EIO;  // Return -EIO on read error
    }

    // Verify the ELF magic number to check if the file is a valid ELF file
    if (ehdr.e_ident[EI_MAG0_IDX] != EI_MAG0 || ehdr.e_ident[EI_MAG1_IDX] != EI_MAG1 ||
        ehdr.e_ident[EI_MAG2_IDX] != EI_MAG2 || ehdr.e_ident[EI_MAG3_IDX] != EI_MAG3) {
        kprintf("Invalid ELF magic number\n");
        return -EBADFMT;
    }else if (ehdr.e_ident[EI_CLASS_IDX] != ELFCLASS64) {
        kprintf("Invalid EI class\n");
        return -EBADFMT;
    }else if (ehdr.e_ident[EI_DATA_IDX] != ELFDATA2LSB) {
        kprintf("Invalid EI data encoding\n");
        return -EBADFMT;
    }else if (ehdr.e_ident[EI_VERSION_IDX] != EI_CURRENT) {
        kprintf("Invalid EI version\n");
        return -EBADFMT;
    }else if (ehdr.e_ident[EI_OSABI_IDX] != ELFOSABI_SYSV &&
              ehdr.e_ident[EI_OSABI_IDX] != ELFOSABI_NONE) {
        kprintf("Invalid EI OSABI\n");
        return -EBADFMT;
    }

    // e_type - excutable file
    if (ehdr.e_type != ET_EXEC) {
        kprintf("Invalid ELF file type - not executable! \n");
        return -EINVAL;
    }

    // e_machine - riscv64
    if (ehdr.e_machine != EM_RISCV64) {
        kprintf("Invalid ELF machine - not RISC-V! \n");
        return -EINVAL;
    }

    // e_version - current version
    if (ehdr.e_version != EV_CURRENT) {
        kprintf("Invalid ELF version - not current! \n");
        return -EINVAL;
    }

    // e_entry - entry point address
    if (!ehdr.e_entry) {
        kprintf("Invalid ELF entry point address\n");
        return -EINVAL;
    }else{
        *entryptr = (void *)ehdr.e_entry;
    }

    // program header table entries
    struct Elf64_Phdr *phdrs = kmalloc(ehdr.e_phentsize * ehdr.e_phnum);
    if (phdrs == NULL) {
        kprintf("Failed to read program headers\n");
        return -EINVAL;  // Return -1 on memory allocation failure
    }

    // Seek to the program header table in the ELF file and read it
    if (ioseek(io, ehdr.e_phoff) < 0 ||
        ioread_full(io, phdrs, ehdr.e_phentsize * ehdr.e_phnum) != (ehdr.e_phentsize * ehdr.e_phnum)) {
        kfree(phdrs);  
        kprintf("Failed to read program headers\n");
        return -EIO;   // Return -EIO on read error
    }

    // Iterate through each program header entry in the program header table
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {  // Only process loadable segments
            kprintf("Allocating memory for segment %d of size %u\n", i, phdrs[i].p_memsz);
            if(phdrs[i].p_vaddr < START_ADDR || phdrs[i].p_vaddr + phdrs[i].p_memsz > END_ADDR){
                kprintf("Invalid segment address\n");
                kfree(phdrs);  
                break;
            }
            // Allocate memory for the segment with the specified memory size

            // Zero out the allocated memory to initialize it
            memset((void*)phdrs[i].p_vaddr, 0, phdrs[i].p_memsz);

            // Load the segment content from the file into the allocated memory
            if (ioseek(io, phdrs[i].p_offset) < 0 ||
                ioread_full(io, (void*)phdrs[i].p_vaddr, phdrs[i].p_filesz) != phdrs[i].p_filesz) {
                kfree(phdrs);    
                return -EIO;     // Return -EIO on read error
            }
        }
    }

    kfree(phdrs);

    return 0;  // Return 0 on successful load
}


