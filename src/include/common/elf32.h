/**************************************************************************
 *   Copyright (C) 2009, 2011 by Hanna Reitz                              *
 *   xanclic@googlemail.com                                               *
 *                                                                        *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the  Free Software Foundation;  either version 2 of the License,  or *
 *   (at your option) any later version.                                  *
 *                                                                        *
 *   This program  is  distributed  in the hope  that it will  be useful, *
 *   but  WITHOUT  ANY  WARRANTY;  without even the  implied  warranty of *
 *   MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 *   GNU General Public License for more details.                         *
 *                                                                        *
 *   You should have received a copy of the  GNU  General  Public License *
 *   along with this program; if not, write to the                        *
 *   Free Software Foundation, Inc.,                                      *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.            *
 **************************************************************************/

#ifndef _ELF32_H
#define _ELF32_H

#include <compiler.h>
#include <stdint.h>

#define EI_NIDENT     16
#define EI_CLASS       4
#define EI_DATA        5
#define EI_VERSION     6
#define ELFCLASS32     1
#define ELFCLASS64     2
#define ELFDATA2LSB    1
#define ELFDATA2MSB    2
#define ET_REL         1
#define ET_EXEC        2
#define ET_DYN         3
#define ET_CORE        4
#define EM_386         3    /*       Intel 80386       */
#define EM_ARM        40    /*           ARM           */
#define EM_PPC        20    /*         PowerPC         */
#define EM_PPC64      21    /*     PowerPC 64-bit      */
#define EM_X86_64     62    /* AMD x86-64 architecture */
#define EV_CURRENT     1
#define SH_PROGBITS    1
#define SH_STRTAB      3
#define PT_LOAD        1
#define PT_DYNAMIC     2
#define PT_INTERP      3
#define SHT_NULL       0
#define SHT_PROGBITS   1
#define SHT_SYMTAB     2
#define SHT_STRTAB     3
#define SHT_RELA       4
#define SHT_HASH       5
#define SHT_DYNAMIC    6
#define SHT_NOTE       7
#define SHT_NOBITS     8
#define SHT_REL        9
#define SHT_SHLIB      10
#define SHT_DYNSYM     11
#define SHF_WRITE      0x01
#define SHF_ALLOC      0x02
#define SHF_EXECINSTR  0x04
#define PF_X           0x01
#define PF_W           0x02
#define PF_R           0x04
#define SH_SYMTAB      2
#define SH_STRTAB      3
#define SHN_UNDEF      0
#define SHN_ABS        0xFFF1
#define SHN_COMMON     0xFFF2
#define ELF32_ST_BIND(i) ((i) >> 4)
#define ELF32_ST_TYPE(i) ((i) & 0xF)
#define ELF32_ST_INFO(b, t) (((b) << 4) + ((t) & 0xF))
#define STT_NOTYPE     0   /* Symbol type is unspecified */
#define STT_OBJECT     1   /* Symbol is a data object */
#define STT_FUNC       2   /* Symbol is a code object */
#define STT_SECTION    3   /* Symbol associated with a section */
#define STT_FILE       4   /* Symbol's name is file name */
#define STT_COMMON     5   /* Symbol is a common data object */
#define STT_TLS        6   /* Symbol is thread-local data object*/
#define STT_NUM        7   /* Number of defined types.  */
#define STT_LOOS      10    /* Start of OS-specific */
#define STT_HIOS      12    /* End of OS-specific */
#define STT_LOPROC    13    /* Start of processor-specific */
#define STT_HIPROC    15    /* End of processor-specific */
#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s, t) (((s) << 8) + (unsigned char)(t))

#define R_ARM_NONE                  0
#define R_ARM_PC24                  1
#define R_ARM_ABS32                 2
#define R_ARM_REL32                 3
#define R_ARM_LDR_PC_G0             4
#define R_ARM_ABS16                 5
#define R_ARM_ABS12                 6
#define R_ARM_THM_ABS5              7
#define R_ARM_ABS8                  8
#define R_ARM_SBREL32               9
#define R_ARM_THM_CALL              10
#define R_ARM_THM_PC8               11
#define R_ARM_BREL_ADJ              12
#define R_ARM_TLS_DESC              13
#define R_ARM_TLS_DTPMOD32          17
#define R_ARM_TLS_DTPOFF32          18
#define R_ARM_TLS_TPOFF32           19
#define R_ARM_COPY                  20
#define R_ARM_GLOB_DAT              21
#define R_ARM_JUMP_SLOT             22
#define R_ARM_RELATIVE              23
#define R_ARM_GOTOFF32              24
#define R_ARM_BASE_PREL             25
#define R_ARM_GOT_BREL              26
#define R_ARM_PLT32                 27
#define R_ARM_CALL                  28
#define R_ARM_JUMP24                29
#define R_ARM_THM_JUMP24            30
#define R_ARM_BASE_ABS              31
#define R_ARM_LDR_SBREL_11_0_NC     35
#define R_ARM_LDR_SBREL_19_12_NC    36
#define R_ARM_LDR_SBREL_27_20_CK    37
#define R_ARM_TARGET1               38
#define R_ARM_SBREL31               39
#define R_ARM_V4BX                  40
#define R_ARM_TARGET2               41
#define R_ARM_PREL31                42

// Definiert den erwarteten Wert für e_machine
#include <arch-elf-machine.h>

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef  int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

typedef struct elf32_hdr
{
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
} cc_packed Elf32_Ehdr;

typedef struct elf32_phdr
{
    Elf32_Word    p_type;
    Elf32_Off     p_offset;
    Elf32_Addr    p_vaddr;
    Elf32_Addr    p_paddr;
    Elf32_Word    p_filesz;
    Elf32_Word    p_memsz;
    Elf32_Word    p_flags;
    Elf32_Word    p_align;
} cc_packed Elf32_Phdr;

typedef struct
{
    Elf32_Word    sh_name;
    Elf32_Word    sh_type;
    Elf32_Word    sh_flags;
    Elf32_Addr    sh_addr;
    Elf32_Off     sh_offset;
    Elf32_Word    sh_size;
    Elf32_Word    sh_link;
    Elf32_Word    sh_info;
    Elf32_Word    sh_addralign;
    Elf32_Word    sh_entsize;
} cc_packed Elf32_Shdr;

typedef struct elf32_sym
{
    Elf32_Word    st_name;
    Elf32_Addr    st_value;
    Elf32_Word    st_size;
    unsigned char st_info;
    unsigned char st_other;
    Elf32_Half    st_shndx;
} cc_packed Elf32_Sym;

typedef struct elf32_dyn
{
    Elf32_Sword d_tag;
    union
    {
        Elf32_Word d_val;
        Elf32_Addr d_ptr;
    } d_un;
} cc_packed Elf32_Dyn;

typedef struct elf32_rel
{
    Elf32_Addr r_offset;
    Elf32_Word r_info;
} cc_packed Elf32_Rel;

typedef struct elf32_rela
{
    Elf32_Addr r_offset;
    Elf32_Word r_info;
    Elf32_Sword r_addend;
} cc_packed Elf32_Rela;

#endif
