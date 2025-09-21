#include "fs.h"

// --- Variáveis Globais (Estado do FS em Memória) ---
Superblock sb;
Inode inodes[NUM_INODES];
unsigned char freespace_bitmap[NUM_BLOCKS / 8];
int current_dir_inode;

// --- Funções Auxiliares ---

void set_bit(int block_num) {
    freespace_bitmap[block_num / 8] |= (1 << (block_num % 8));
}

void clear_bit(int block_num) {
    freespace_bitmap[block_num / 8] &= ~(1 << (block_num % 8));
}

int get_bit(int block_num) {
    return (freespace_bitmap[block_num / 8] & (1 << (block_num % 8))) != 0;
}

int find_free_block() {
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (!get_bit(i)) return i;
    }
    return -1;
}

int find_free_inode() {
    for (int i = 0; i < NUM_INODES; i++) {
        if (inodes[i].type == '0') return i;
    }
    return -1;
}

void read_block(int block_num, void* buffer) {
    char path[256];
    sprintf(path, "fs/blocks/%d.dat", block_num);
    FILE* f = fopen(path, "rb");
    if (!f) { perror("read_block"); exit(1); }
    fread(buffer, BLOCK_SIZE, 1, f);
    fclose(f);
}

void write_block(int block_num, void* buffer) {
    char path[256];
    sprintf(path, "fs/blocks/%d.dat", block_num);
    FILE* f = fopen(path, "wb");
    if (!f) { perror("write_block"); exit(1); }
    fwrite(buffer, BLOCK_SIZE, 1, f);
    fclose(f);
}

// --- Implementação dos comandos ---

void fs_mount() {
    FILE* f;
    if ((f = fopen("fs/superblock.dat", "rb"))) {
        fread(&sb, sizeof(Superblock), 1, f);
        fclose(f);
    } else {
        printf("Sistema de arquivos não encontrado. Execute o programa 'format' primeiro.\n");
        exit(1);
    }

    if ((f = fopen("fs/inodes.dat", "rb"))) {
        fread(inodes, sizeof(Inode), NUM_INODES, f);
        fclose(f);
    } else {
       printf("inodes.dat não encontrado!\n"); exit(1);
    }

    if ((f = fopen("fs/freespace.dat", "rb"))) {
        fread(freespace_bitmap, sizeof(freespace_bitmap), 1, f);
        fclose(f);
    } else {
        printf("freespace.dat não encontrado!\n"); exit(1);
    }
    
    current_dir_inode = 0; // Começa no diretório raiz
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

void fs_mkdir(const char* name) {
    int new_inode_num = find_free_inode();
    int new_block_num = find_free_block();


    if (new_inode_num == -1 || new_block_num == -1) {
        printf("Erro: Nao ha espaco ou i-nodes livres.\n");
        return;
    }

    Inode* parent_inode = &inodes[current_dir_inode];
    char block_buffer[BLOCK_SIZE];
    read_block(parent_inode->direct_pointers[0], block_buffer);
    
    DirectoryEntry* de = (DirectoryEntry*)block_buffer;
    int entry_count = parent_inode->size / sizeof(DirectoryEntry);
    de[entry_count].inode_number = new_inode_num;
    strncpy(de[entry_count].name, name, MAX_FILENAME);
    write_block(parent_inode->direct_pointers[0], block_buffer);
    parent_inode->size += sizeof(DirectoryEntry);

    inodes[new_inode_num].type = 'd';
    inodes[new_inode_num].size = 2 * sizeof(DirectoryEntry);
    inodes[new_inode_num].direct_pointers[0] = new_block_num;
    set_bit(new_block_num);
    
    DirectoryEntry new_dir_entries[2];
    strcpy(new_dir_entries[0].name, ".");
    new_dir_entries[0].inode_number = new_inode_num;
    strcpy(new_dir_entries[1].name, "..");
    new_dir_entries[1].inode_number = current_dir_inode;

    memset(block_buffer, 0, BLOCK_SIZE);
    memcpy(block_buffer, new_dir_entries, 2 * sizeof(DirectoryEntry));
    write_block(new_block_num, block_buffer);
}

void fs_ls() {
    Inode* dir_inode = &inodes[current_dir_inode];
    char block_buffer[BLOCK_SIZE];
    read_block(dir_inode->direct_pointers[0], block_buffer);

    DirectoryEntry* de = (DirectoryEntry*)block_buffer;
    int entry_count = dir_inode->size / sizeof(DirectoryEntry);

    for (int i = 0; i < entry_count; i++) {
        Inode* entry_inode = &inodes[de[i].inode_number];
        printf("%c - %-4d %-20s %d\n", 
               entry_inode->type, 
               de[i].inode_number, 
               de[i].name, 
               entry_inode->size);
    }
}

void fs_touch(const char* name) {
    printf("Digite o conteudo do arquivo (CTRL+D no Linux/macOS ou CTRL+Z no Windows para terminar):\n"); // Dar enter antes de CTRL+D/Z
    char buffer[1024] = {0}; 
    int bytes_read = 0;
    char c;
    while((c = getchar()) != EOF && bytes_read < 1023) {
        buffer[bytes_read++] = c;
    }

    int new_inode_num = find_free_inode();
    if (new_inode_num == -1) { printf("Erro: Sem i-nodes livres.\n"); return; }

    inodes[new_inode_num].type = 'f';
    inodes[new_inode_num].size = bytes_read;

    int bytes_to_write = bytes_read;
    for(int i = 0; i < NUM_DIRECT_POINTERS && bytes_to_write > 0; i++) {
        int block_num = find_free_block();
        if (block_num == -1) { printf("Erro: Sem blocos livres.\n"); return; }
        set_bit(block_num);
        inodes[new_inode_num].direct_pointers[i] = block_num;
        
        int chunk_size = (bytes_to_write > BLOCK_SIZE) ? BLOCK_SIZE : bytes_to_write;
        write_block(block_num, buffer + (bytes_read - bytes_to_write));
        bytes_to_write -= chunk_size;
    }

    // Adicionar entrada no diretório atual
    Inode* parent_inode = &inodes[current_dir_inode];
    char block_buffer_dir[BLOCK_SIZE];
    read_block(parent_inode->direct_pointers[0], block_buffer_dir);
    
    DirectoryEntry* de = (DirectoryEntry*)block_buffer_dir;
    int entry_count = parent_inode->size / sizeof(DirectoryEntry);
    de[entry_count].inode_number = new_inode_num;

    strncpy(de[entry_count].name, name, MAX_FILENAME);
    write_block(parent_inode->direct_pointers[0], block_buffer_dir);
    parent_inode->size += sizeof(DirectoryEntry);
}

void fs_stat() {
    int free_blocks = 0;
    for(int i = 0; i < NUM_BLOCKS; i++) {
        if (!get_bit(i)) {
            free_blocks++;
        }
    }
    printf("Espaco livre: %d Bytes\n", free_blocks * BLOCK_SIZE);
    printf("Blocos livres: %d Blocos\n", free_blocks);
    printf("Tamanho do bloco: %d Bytes\n", BLOCK_SIZE);
}

void fs_cd(const char* path) {
    // Tratamento especial para ir direto para a raiz
    if (strcmp(path, "/") == 0) {
        current_dir_inode = 0;
        return;
    }

    // 1. Acessa o i-node e o bloco de dados do diretório atual
    Inode* dir_inode = &inodes[current_dir_inode];
    char block_buffer[BLOCK_SIZE];
    read_block(dir_inode->direct_pointers[0], block_buffer);

    DirectoryEntry* de = (DirectoryEntry*)block_buffer;
    int entry_count = dir_inode->size / sizeof(DirectoryEntry);

    // 2. Procura pelo nome do diretório de destino
    for (int i = 0; i < entry_count; i++) {
        if (strcmp(de[i].name, path) == 0) {
            Inode* target_inode = &inodes[de[i].inode_number];
            // 3. Verifica se é um diretório
            if (target_inode->type == 'd') {
                // 4. Atualiza o diretório atual
                current_dir_inode = de[i].inode_number;
                return;
            } else {
                printf("Erro: '%s' nao eh um diretorio.\n", path);
                return;
            }
        }
    }

    // 5. Se o loop terminar, o diretório não foi encontrado
    printf("Erro: Diretorio '%s' nao encontrado.\n", path);
}

void fs_get_current_path(char* path_buffer) {
    if (current_dir_inode == 0) {
        strcpy(path_buffer, "~");
        return;
    }

    char path_parts[32][MAX_FILENAME];
    int depth = 0;
    int temp_inode_num = current_dir_inode;

    while (temp_inode_num != 0) {
        Inode* current_node = &inodes[temp_inode_num];
        char current_block[BLOCK_SIZE];
        read_block(current_node->direct_pointers[0], current_block);
        DirectoryEntry* current_de = (DirectoryEntry*)current_block;
        
        int parent_inode_num = -1;
        if (strcmp(current_de[1].name, "..") == 0) {
             parent_inode_num = current_de[1].inode_number;
        }

        if (parent_inode_num == temp_inode_num) break;

        Inode* parent_node = &inodes[parent_inode_num];
        char parent_block[BLOCK_SIZE];
        read_block(parent_node->direct_pointers[0], parent_block);
        DirectoryEntry* parent_de = (DirectoryEntry*)parent_block;
        int parent_entry_count = parent_node->size / sizeof(DirectoryEntry);

        for (int i = 0; i < parent_entry_count; i++) {
            if (parent_de[i].inode_number == temp_inode_num) {
                strcpy(path_parts[depth], parent_de[i].name);
                depth++;
                break;
            }
        }
        temp_inode_num = parent_inode_num;
    }

    // Constrói a string final do caminho
    strcpy(path_buffer, "~");
    for (int i = depth - 1; i >= 0; i--) {
        strcat(path_buffer, "/");
        strcat(path_buffer, path_parts[i]);
    }
    // Se o buffer estiver vazio, significa que estamos na raiz
    if (strlen(path_buffer) == 0) {
        strcpy(path_buffer, "~/");
    }
}

void fs_pwd() {
    char current_path[256];
    fs_get_current_path(current_path);
    printf("%s\n", current_path);
}


void fs_cat(const char* name) { printf("Comando 'cat' ainda nao implementado.\n"); }
void fs_rm(const char* name) { printf("Comando 'rm' ainda nao implementado.\n"); }

// --- CONTEÚDO DE MAIN.C ---

#define MAX_CMD_LEN 256
#define MAX_ARGS 10

int main() {
    char cmd_line[MAX_CMD_LEN];
    char* args[MAX_ARGS];
    int nargs;

    fs_mount();

    while (1) {
        char prompt[MAX_CMD_LEN];
        char current_path[MAX_CMD_LEN];
        fs_get_current_path(current_path);
        sprintf(prompt, "kleberfs:%s$ ", current_path);
       
        printf("%s", prompt);
        fflush(stdout);

        if (fgets(cmd_line, sizeof(cmd_line), stdin) == NULL) {
            printf("\n");
            break; // EOF (CTRL+D ou CTRL+Z)
        }

        nargs = 0;
        args[nargs] = strtok(cmd_line, " \t\n");
        while (args[nargs] != NULL && nargs < MAX_ARGS - 1) {
            nargs++;
            args[nargs] = strtok(NULL, " \t\n");
        }

        if (nargs == 0) {
            continue;
        }

        if (strcmp(args[0], "exit") == 0) {
            break;
        } else if (strcmp(args[0], "mkdir") == 0) {
            if (nargs > 1) fs_mkdir(args[1]);
            else printf("Uso: mkdir <nomedodiretorio>\n");
        } else if (strcmp(args[0], "ls") == 0) {
            fs_ls();
        } else if (strcmp(args[0], "touch") == 0) {
            if (nargs > 1) fs_touch(args[1]);
            else printf("Uso: touch <nomedoarquivo>\n");
        } else if (strcmp(args[0], "stat") == 0) {
            fs_stat();
        } else if (strcmp(args[0], "cd") == 0) {
            if (nargs > 1) fs_cd(args[1]);
            else printf("Uso: cd <caminho>\n");
        } else if (strcmp(args[0], "pwd") == 0) {
            fs_pwd();
        } else if (strcmp(args[0], "cat") == 0) {
            if (nargs > 1) fs_cat(args[1]);
            else printf("Uso: cat <nomedoarquivo>\n");
        } else if (strcmp(args[0], "rm") == 0) {
            if (nargs > 1) fs_rm(args[1]);
            else printf("Uso: rm <nome>\n");
        } else if (strcmp(args[0], "clear") == 0 || strcmp(args[0], "cls") == 0) {
            system("cls");
        }
        else {
            printf("Comando desconhecido: %s\n", args[0]);
        }
        
        // fs_unmount();
        // fs_mount();
    }

    fs_unmount();
    return 0;
}