#include "fs.h"

// Arquivo de Formatação inicial do Sistema de Arquivos

Superbloco superbloco;
Inode inodes[NUM_INODES];
unsigned char mapa_espaco_livre[NUM_BLOCKS / 8];

void escrever_bloco(int num_bloco, void* buffer) {
    char caminho[256];
    sprintf(caminho, "fs/blocks/%d.dat", num_bloco);
    FILE* f = fopen(caminho, "wb");
    if (!f) { perror("escrever_bloco"); exit(1); }
    fwrite(buffer, BLOCK_SIZE, 1, f);
    fclose(f);
}

void definir_bit(int num_bloco) {
    mapa_espaco_livre[num_bloco / 8] |= (1 << (num_bloco % 8));
}

int encontrar_bloco_livre() {
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (!((mapa_espaco_livre[i / 8] & (1 << (i % 8))) != 0)) return i;
    }
    return -1;
}

int encontrar_inode_livre() {
    for (int i = 0; i < NUM_INODES; i++) {
        if (inodes[i].type == '0') return i;
    }
    return -1;
}

void fs_desmontar() {
    FILE* f;
    f = fopen("fs/inodes.dat", "wb");
    fwrite(inodes, sizeof(Inode), NUM_INODES, f);
    fclose(f);
    f = fopen("fs/freespace.dat", "wb");
    fwrite(mapa_espaco_livre, sizeof(mapa_espaco_livre), 1, f);
    fclose(f);
}

void fs_formatar() {
    mkdir("fs");
    mkdir("fs/blocks");

    strcpy(superbloco.filesystem, FILESYSTEM_NAME);
    superbloco.blocksize = BLOCK_SIZE;
    superbloco.partitionsize = PARTITION_SIZE;
    superbloco.num_blocks = NUM_BLOCKS;
    superbloco.num_inodes = NUM_INODES;
    FILE* f_super = fopen("fs/Superbloco.dat", "wb");
    fwrite(&superbloco, sizeof(Superbloco), 1, f_super);
    fclose(f_super);

    memset(inodes, 0, sizeof(inodes));
    for (int i = 0; i < NUM_INODES; i++) {
        inodes[i].type = '0';
    }
    FILE* f_inodes = fopen("fs/inodes.dat", "wb");
    fwrite(inodes, sizeof(Inode), NUM_INODES, f_inodes);
    fclose(f_inodes);
    
    memset(mapa_espaco_livre, 0, sizeof(mapa_espaco_livre));
    FILE* f_free = fopen("fs/freespace.dat", "wb");
    fwrite(mapa_espaco_livre, sizeof(mapa_espaco_livre), 1, f_free);
    fclose(f_free);

    char block_path[256];
    char empty_block[BLOCK_SIZE] = {0};
    for (int i = 0; i < NUM_BLOCKS; i++) {
        sprintf(block_path, "fs/blocks/%d.dat", i);
        FILE* f_block = fopen(block_path, "wb");
        fwrite(empty_block, BLOCK_SIZE, 1, f_block);
        fclose(f_block);
    }

    int inode_raiz = encontrar_inode_livre();
    int bloco_raiz = encontrar_bloco_livre();
    
    inodes[inode_raiz].type = 'd';
    inodes[inode_raiz].size = 2 * sizeof(EntradaDiretorio);
    inodes[inode_raiz].direct_pointers[0] = bloco_raiz;
    definir_bit(bloco_raiz);

    EntradaDiretorio entradas[2];
    strcpy(entradas[0].name, ".");
    entradas[0].inode_number = inode_raiz;
    strcpy(entradas[1].name, "..");
    entradas[1].inode_number = inode_raiz;

    char buffer_bloco[BLOCK_SIZE] = {0};
    memcpy(buffer_bloco, entradas, 2 * sizeof(EntradaDiretorio));
    escrever_bloco(bloco_raiz, buffer_bloco);
    
    fs_desmontar();
}


int main() {
    printf("Formatando o sistema de arquivos ...\n");
    fs_formatar();
    printf("Formatacao concluida com sucesso!\n");
    return 0;
}