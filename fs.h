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

// Funções de formatação e montagem (nomes em português)
void fs_formatar();
void fs_montar();
void fs_desmontar();

// Funções de manipulação de diretório/arquivo (nomes em português)
void fs_criar_diretorio(const char* nome);
void fs_mudar_diretorio(const char* caminho);
void fs_mostrar_caminho();
void fs_criar_arquivo(const char* nome);
void fs_mostrar_arquivo(const char* nome);
void fs_listar();
void fs_remover(const char* nome);
void fs_estado();

#endif // FS_H