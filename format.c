#include "fs.h"

// Arquivo de Formatação inicial do Sistema de Arquivos

Superblock sb;
Inode inodes[NUM_INODES];
unsigned char freespace_bitmap[NUM_BLOCKS / 8];

void write_block(int block_num, void* buffer) {
    char path[256];
    sprintf(path, "fs/blocks/%d.dat", block_num);
    FILE* f = fopen(path, "wb");
    if (!f) { perror("write_block"); exit(1); }
    fwrite(buffer, BLOCK_SIZE, 1, f);
    fclose(f);
}

void set_bit(int block_num) {
    freespace_bitmap[block_num / 8] |= (1 << (block_num % 8));
}

int find_free_block() {
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (!((freespace_bitmap[i / 8] & (1 << (i % 8))) != 0)) return i;
    }
    return -1;
}

int find_free_inode() {
    for (int i = 0; i < NUM_INODES; i++) {
        if (inodes[i].type == '0') return i;
    }
    return -1;
}

void fs_unmount() {
    FILE* f;
    f = fopen("fs/inodes.dat", "wb");
    fwrite(inodes, sizeof(Inode), NUM_INODES, f);
    fclose(f);
    f = fopen("fs/freespace.dat", "wb");
    fwrite(freespace_bitmap, sizeof(freespace_bitmap), 1, f);
    fclose(f);
}

void fs_format() {
    mkdir("fs");
    mkdir("fs/blocks");

    strcpy(sb.filesystem, FILESYSTEM_NAME);
    sb.blocksize = BLOCK_SIZE;
    sb.partitionsize = PARTITION_SIZE;
    sb.num_blocks = NUM_BLOCKS;
    sb.num_inodes = NUM_INODES;
    FILE* f_super = fopen("fs/superblock.dat", "wb");
    fwrite(&sb, sizeof(Superblock), 1, f_super);
    fclose(f_super);

    memset(inodes, 0, sizeof(inodes));
    for (int i = 0; i < NUM_INODES; i++) {
        inodes[i].type = '0';
    }
    FILE* f_inodes = fopen("fs/inodes.dat", "wb");
    fwrite(inodes, sizeof(Inode), NUM_INODES, f_inodes);
    fclose(f_inodes);
    
    memset(freespace_bitmap, 0, sizeof(freespace_bitmap));
    FILE* f_free = fopen("fs/freespace.dat", "wb");
    fwrite(freespace_bitmap, sizeof(freespace_bitmap), 1, f_free);
    fclose(f_free);

    char block_path[256];
    char empty_block[BLOCK_SIZE] = {0};
    for (int i = 0; i < NUM_BLOCKS; i++) {
        sprintf(block_path, "fs/blocks/%d.dat", i);
        FILE* f_block = fopen(block_path, "wb");
        fwrite(empty_block, BLOCK_SIZE, 1, f_block);
        fclose(f_block);
    }

    int root_inode_num = find_free_inode();
    int root_block_num = find_free_block();
    
    inodes[root_inode_num].type = 'd';
    inodes[root_inode_num].size = 2 * sizeof(DirectoryEntry);
    inodes[root_inode_num].direct_pointers[0] = root_block_num;
    set_bit(root_block_num);

    DirectoryEntry entries[2];
    strcpy(entries[0].name, ".");
    entries[0].inode_number = root_inode_num;
    strcpy(entries[1].name, "..");
    entries[1].inode_number = root_inode_num;

    char block_buffer[BLOCK_SIZE] = {0};
    memcpy(block_buffer, entries, 2 * sizeof(DirectoryEntry));
    write_block(root_block_num, block_buffer);
    
    fs_unmount();
}


int main() {
    printf("Formatando o sistema de arquivos 'kleberfs'...\n");
    fs_format();
    printf("Formatacao concluida com sucesso!\n");
    return 0;
}