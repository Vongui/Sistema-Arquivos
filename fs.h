#ifndef FS_H
#define FS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// --- Constantes do Sistema de Arquivos ---
#define FILESYSTEM_NAME "kleberfs"
#define BLOCK_SIZE 128
#define PARTITION_SIZE 10240
#define NUM_BLOCKS (PARTITION_SIZE / BLOCK_SIZE)
#define NUM_INODES NUM_BLOCKS
#define MAX_FILENAME 14
#define NUM_DIRECT_POINTERS 8

typedef struct {
    char filesystem[14];
    int blocksize;
    int partitionsize;
    int num_blocks;
    int num_inodes;
} Superblock;

typedef struct {
    char type; // 'f' (file), 'd' (directory), '0' (free)
    int size;
    int direct_pointers[NUM_DIRECT_POINTERS];
} Inode;

typedef struct {
    char name[MAX_FILENAME];
    int inode_number;
} DirectoryEntry;

// --- API do Sistema de Arquivos ---

// Funções de formatação e montagem
void fs_format();
void fs_mount();
void fs_unmount();

// Funções de manipulação de diretório/arquivo
void fs_mkdir(const char* name);
void fs_cd(const char* path);
void fs_pwd();
void fs_touch(const char* name);
void fs_cat(const char* name);
void fs_ls();
void fs_rm(const char* name);
void fs_stat();

#endif // FS_H