#include "fs.h"

Superbloco superbloco;
Inode inodes[NUM_INODES];
unsigned char mapa_espaco_livre[NUM_BLOCKS / 8];
int inode_diretorio_atual;

void definir_bit(int num_bloco) {
    mapa_espaco_livre[num_bloco / 8] |= (1 << (num_bloco % 8));
}

void limpar_bit(int num_bloco) {
    mapa_espaco_livre[num_bloco / 8] &= ~(1 << (num_bloco % 8));
}

int obter_bit(int num_bloco) {
    return (mapa_espaco_livre[num_bloco / 8] & (1 << (num_bloco % 8))) != 0;
}

int encontrar_bloco_livre() {
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (!obter_bit(i)) return i;
    }
    return -1;
}

int encontrar_inode_livre() {
    for (int i = 0; i < NUM_INODES; i++) {
        if (inodes[i].type == '0') return i;
    }
    return -1;
}

int comparar_entradas(const void* a, const void* b) {
    EntradaDiretorio* entradaA = (EntradaDiretorio*)a;
    EntradaDiretorio* entradaB = (EntradaDiretorio*)b;
    return strcmp(entradaA->name, entradaB->name);
}

int encontrar_indice_entrada_por_nome(const char* nome) {
    Inode* inode_dir = &inodes[inode_diretorio_atual];
    
    if (inode_dir->size == 0) return -1;
    
    char buffer_bloco[BLOCK_SIZE];
    ler_bloco(inode_dir->direct_pointers[0], buffer_bloco); 

    EntradaDiretorio* entradas = (EntradaDiretorio*)buffer_bloco;
    int conta_entradas = inode_dir->size / sizeof(EntradaDiretorio);

    for (int i = 0; i < conta_entradas; i++) {
        if (strcmp(entradas[i].name, nome) == 0) {
            return i;
        }
    }

    return -1;
}

void ler_bloco(int num_bloco, void* buffer) {
    char caminho[256];
    sprintf(caminho, "fs/blocks/%d.dat", num_bloco);
    FILE* f = fopen(caminho, "rb");
    if (!f) { perror("ler_bloco"); 
        exit(1); 
    }
    fread(buffer, BLOCK_SIZE, 1, f);
    fclose(f);
}

void escrever_bloco(int num_bloco, void* buffer) {
    char caminho[256];
    sprintf(caminho, "fs/blocks/%d.dat", num_bloco);
    FILE* f = fopen(caminho, "wb");
    if (!f) { perror("escrever_bloco"); 
        exit(1); 
    }
    fwrite(buffer, BLOCK_SIZE, 1, f);
    fclose(f);
}

void fs_montar() {
    FILE* f;
    if ((f = fopen("fs/Superbloco.dat", "rb"))) {
        fread(&superbloco, sizeof(Superbloco), 1, f);
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
        fread(mapa_espaco_livre, sizeof(mapa_espaco_livre), 1, f);
        fclose(f);
    } else {
        printf("freespace.dat não encontrado!\n"); exit(1);
    }
    
    inode_diretorio_atual = 0; // Começa no diretório raiz
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

void fs_criar_diretorio(char* nome) {

    if (encontrar_indice_entrada_por_nome(nome) != -1)
    {
        printf("Erro: Diretorio '%s' ja existe.\n", nome);
        return;
    }

    int novo_num_inode = encontrar_inode_livre();
    int novo_num_bloco = encontrar_bloco_livre();

    if (novo_num_inode == -1 || novo_num_bloco == -1) {
        printf("Erro: Nao ha espaco ou i-nodes livres.\n");
        return;
    }

    Inode* inode_pai = &inodes[inode_diretorio_atual];
    char buffer_bloco[BLOCK_SIZE];
    ler_bloco(inode_pai->direct_pointers[0], buffer_bloco);
    
    EntradaDiretorio* entradas = (EntradaDiretorio*)buffer_bloco;
    int conta_entradas = inode_pai->size / sizeof(EntradaDiretorio);
    entradas[conta_entradas].inode_number = novo_num_inode;
    strncpy(entradas[conta_entradas].name, nome, MAX_FILENAME);
    escrever_bloco(inode_pai->direct_pointers[0], buffer_bloco);
    inode_pai->size += sizeof(EntradaDiretorio);

    inodes[novo_num_inode].type = 'd';
    inodes[novo_num_inode].size = 2 * sizeof(EntradaDiretorio);
    inodes[novo_num_inode].direct_pointers[0] = novo_num_bloco;
    definir_bit(novo_num_bloco);
    
    EntradaDiretorio novas_entradas[2];
    strcpy(novas_entradas[0].name, ".");
    novas_entradas[0].inode_number = novo_num_inode;
    strcpy(novas_entradas[1].name, "..");
    novas_entradas[1].inode_number = inode_diretorio_atual;

    memset(buffer_bloco, 0, BLOCK_SIZE);
    memcpy(buffer_bloco, novas_entradas, 2 * sizeof(EntradaDiretorio));
    escrever_bloco(novo_num_bloco, buffer_bloco);

    fs_desmontar();
}

void fs_listar() {
    Inode* inode_dir = &inodes[inode_diretorio_atual];
    char buffer_bloco[BLOCK_SIZE];
    
    ler_bloco(inode_dir->direct_pointers[0], buffer_bloco);

    EntradaDiretorio* entradas_originais = (EntradaDiretorio*)buffer_bloco;
    int conta_entradas = inode_dir->size / sizeof(EntradaDiretorio);

    if (conta_entradas == 0) {
        return;
    }

    EntradaDiretorio entradas_para_ordenar[conta_entradas];
    
    memcpy(entradas_para_ordenar, entradas_originais, conta_entradas * sizeof(EntradaDiretorio));

    qsort(entradas_para_ordenar, conta_entradas, sizeof(EntradaDiretorio), comparar_entradas);

    for (int i = 0; i < conta_entradas; i++) {
        Inode* inode_entrada = &inodes[entradas_para_ordenar[i].inode_number];
        printf("%c - %-4d %-20s %d\n", 
                inode_entrada->type, 
                entradas_para_ordenar[i].inode_number, 
                entradas_para_ordenar[i].name, 
                inode_entrada->size);
    }
}

void fs_criar_arquivo(char* nome) {

    if (encontrar_indice_entrada_por_nome(nome) != -1) {
        printf("Erro: Arquivo '%s' ja existe.\n", nome);
        return;
    }

    printf("Digite o conteudo do arquivo (CTRL+D no Linux/macOS ou CTRL+Z no Windows para terminar):\n");
    char buffer[1024] = {0}; 
    int bytes_lidos = 0;
    char c;
    while((c = getchar()) != EOF && bytes_lidos < 1023) {
        buffer[bytes_lidos++] = c;
    }

    int novo_num_inode = encontrar_inode_livre();
    if (novo_num_inode == -1) { 
        printf("Erro: Sem i-nodes livres.\n"); 
        return; 
    }

    inodes[novo_num_inode].type = 'f';
    inodes[novo_num_inode].size = bytes_lidos;

    int bytes_para_escrever = bytes_lidos;
    for(int i = 0; i < NUM_DIRECT_POINTERS && bytes_para_escrever > 0; i++) {
        int num_bloco = encontrar_bloco_livre();
        if (num_bloco == -1) { 
            printf("Erro: Sem blocos livres.\n"); 
            return; 
        }
        definir_bit(num_bloco);
        inodes[novo_num_inode].direct_pointers[i] = num_bloco;
        
        int tamanho_chunk = (bytes_para_escrever > BLOCK_SIZE) ? BLOCK_SIZE : bytes_para_escrever;
        escrever_bloco(num_bloco, buffer + (bytes_lidos - bytes_para_escrever));
        bytes_para_escrever -= tamanho_chunk;
    }

    Inode* inode_pai = &inodes[inode_diretorio_atual];
    char buffer_bloco_dir[BLOCK_SIZE];
    ler_bloco(inode_pai->direct_pointers[0], buffer_bloco_dir);
    
    EntradaDiretorio* entradas = (EntradaDiretorio*)buffer_bloco_dir;
    int conta_entradas = inode_pai->size / sizeof(EntradaDiretorio);
    entradas[conta_entradas].inode_number = novo_num_inode;

    strncpy(entradas[conta_entradas].name, nome, MAX_FILENAME);
    escrever_bloco(inode_pai->direct_pointers[0], buffer_bloco_dir);
    inode_pai->size += sizeof(EntradaDiretorio);

    fs_desmontar();
}

void fs_estado() {
    int blocos_livres = 0;
    for(int i = 0; i < NUM_BLOCKS; i++) {
        if (!obter_bit(i)) {
            blocos_livres++;
        }
    }
    printf("Espaco livre: %d Bytes\n", blocos_livres * BLOCK_SIZE);
    printf("Blocos livres: %d Blocos\n", blocos_livres);
    printf("Tamanho do bloco: %d Bytes\n", BLOCK_SIZE);
}

void fs_mudar_diretorio(char* caminho) {
    if (strcmp(caminho, "/") == 0) {
        inode_diretorio_atual = 0;
        return;
    }

    Inode* inode_dir = &inodes[inode_diretorio_atual];
    char buffer_bloco[BLOCK_SIZE];
    ler_bloco(inode_dir->direct_pointers[0], buffer_bloco);

    EntradaDiretorio* entradas = (EntradaDiretorio*)buffer_bloco;
    int conta_entradas = inode_dir->size / sizeof(EntradaDiretorio);

    for (int i = 0; i < conta_entradas; i++) {
        if (strcmp(entradas[i].name, caminho) == 0) {
            Inode* inode_alvo = &inodes[entradas[i].inode_number];
            if (inode_alvo->type == 'd') {
                inode_diretorio_atual = entradas[i].inode_number;
                return;
            } else {
                printf("Erro: '%s' nao eh um diretorio.\n", caminho);
                return;
            }
        }
    }

    printf("Erro: Diretorio '%s' nao encontrado.\n", caminho);
}

void fs_obter_caminho_atual(char* buffer_caminho) {
    if (inode_diretorio_atual == 0) {
        strcpy(buffer_caminho, "~");
        return;
    }

    char partes_caminho[32][MAX_FILENAME];
    int profundidade = 0;
    int inode_temp = inode_diretorio_atual;

    while (inode_temp != 0) {
        Inode* no_atual = &inodes[inode_temp];
        char bloco_atual[BLOCK_SIZE];
        ler_bloco(no_atual->direct_pointers[0], bloco_atual);
        EntradaDiretorio* entradas_atual = (EntradaDiretorio*)bloco_atual;
        
        int inode_pai = -1;
        if (strcmp(entradas_atual[1].name, "..") == 0) {
             inode_pai = entradas_atual[1].inode_number;
        }

        if (inode_pai == inode_temp) break;

        Inode* no_pai = &inodes[inode_pai];
        char bloco_pai[BLOCK_SIZE];
        ler_bloco(no_pai->direct_pointers[0], bloco_pai);
        EntradaDiretorio* entradas_pai = (EntradaDiretorio*)bloco_pai;
        int conta_entradas_pai = no_pai->size / sizeof(EntradaDiretorio);

        for (int i = 0; i < conta_entradas_pai; i++) {
            if (entradas_pai[i].inode_number == inode_temp) {
                strcpy(partes_caminho[profundidade], entradas_pai[i].name);
                profundidade++;
                break;
            }
        }
        inode_temp = inode_pai;
    }

    strcpy(buffer_caminho, "~");
    for (int i = profundidade - 1; i >= 0; i--) {
        strcat(buffer_caminho, "/");
        strcat(buffer_caminho, partes_caminho[i]);
    }

    if (strlen(buffer_caminho) == 0) {
        strcpy(buffer_caminho, "~/");
    }
}

void fs_mostrar_caminho() {
    char caminho_atual[256];
    fs_obter_caminho_atual(caminho_atual);
    printf("%s\n", caminho_atual);
}

void fs_mostrar_arquivo(char* nome) {
    Inode* inode_dir = &inodes[inode_diretorio_atual];
    char buffer_bloco_dir[BLOCK_SIZE];
    ler_bloco(inode_dir->direct_pointers[0], buffer_bloco_dir);

    EntradaDiretorio* entradas = (EntradaDiretorio*)buffer_bloco_dir;
    int conta_entradas = inode_dir->size / sizeof(EntradaDiretorio);
    int num_inode_arquivo = -1;

    for (int i = 0; i < conta_entradas; i++) {
        if (strcmp(entradas[i].name, nome) == 0) {
            num_inode_arquivo = entradas[i].inode_number;
            break;
        }
    }

    if (num_inode_arquivo == -1) {
        printf("Erro: Arquivo '%s' nao encontrado.\n", nome);
        return;
    }

    Inode* inode_arquivo = &inodes[num_inode_arquivo];
    if (inode_arquivo->type != 'f') {
        printf("Erro: '%s' nao eh um arquivo.\n", nome);
        return;
    }

    char buffer_conteudo[BLOCK_SIZE];
    int bytes_restantes = inode_arquivo->size;

    for (int i = 0; i < NUM_DIRECT_POINTERS && bytes_restantes > 0; i++) {
        int num_bloco = inode_arquivo->direct_pointers[i];
        if (num_bloco == 0) continue; // Pula ponteiros não utilizados

        ler_bloco(num_bloco, buffer_conteudo);

        int bytes_para_ler = (bytes_restantes > BLOCK_SIZE) ? BLOCK_SIZE : bytes_restantes;
        fwrite(buffer_conteudo, 1, bytes_para_ler, stdout);
        bytes_restantes -= bytes_para_ler;
    }
    printf("\n");
}

void fs_remover(char* nome) {
    if (strcmp(nome, ".") == 0 || strcmp(nome, "..") == 0) {
        printf("Erro: Nao eh possivel remover '.' ou '..'.\n");
        return;
    }

    int idx_alvo = encontrar_indice_entrada_por_nome(nome);
    if (idx_alvo == -1) {
        printf("Erro: Arquivo ou diretorio '%s' nao encontrado.\n", nome);
        return;
    }

    Inode* inode_pai = &inodes[inode_diretorio_atual];
    char buffer_bloco_pai[BLOCK_SIZE];
    int num_bloco_pai = inode_pai->direct_pointers[0];
    
    ler_bloco(num_bloco_pai, buffer_bloco_pai);
    EntradaDiretorio* entradas = (EntradaDiretorio*)buffer_bloco_pai;
    
    int num_inode_alvo = entradas[idx_alvo].inode_number;
    Inode* inode_alvo = &inodes[num_inode_alvo];

    if (inode_alvo->type == 'f') {
        for (int i = 0; i < NUM_DIRECT_POINTERS; i++) {
            if (inode_alvo->direct_pointers[i] != 0) {
                limpar_bit(inode_alvo->direct_pointers[i]);
            }
        }
    } else if (inode_alvo->type == 'd') {
        if (inode_alvo->size > 2 * sizeof(EntradaDiretorio)) {
            printf("Erro: O diretorio '%s' nao esta vazio.\n", nome);
            return;
        }
        limpar_bit(inode_alvo->direct_pointers[0]);
    }

    memset(inode_alvo, 0, sizeof(Inode));
    inode_alvo->type = '0';

    int conta_entradas_pai = inode_pai->size / sizeof(EntradaDiretorio);
    int idx_ultimo = conta_entradas_pai - 1;

    entradas[idx_alvo] = entradas[idx_ultimo];

    memset(&entradas[idx_ultimo], 0, sizeof(EntradaDiretorio)); 

    inode_pai->size -= sizeof(EntradaDiretorio);

    escrever_bloco(num_bloco_pai, buffer_bloco_pai);

    fs_desmontar();
    printf("'%s' removido com sucesso.\n", nome);
}

#define MAX_CMD_LEN 256
#define MAX_ARGS 10

int main() {
    char linha_cmd[MAX_CMD_LEN];
    char* args[MAX_ARGS];
    int nargs;

    fs_montar();

    while (1) {
        char prompt[MAX_CMD_LEN];
        char caminho_atual[MAX_CMD_LEN];
        fs_obter_caminho_atual(caminho_atual);
        sprintf(prompt, "FS@BCC-PC:%s$ ", caminho_atual);
       
        printf("%s", prompt);
        fflush(stdout);

        if (fgets(linha_cmd, sizeof(linha_cmd), stdin) == NULL) {
            printf("\n");
            break;
        }

        nargs = 0;
        args[nargs] = strtok(linha_cmd, " \t\n");
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
            if (nargs > 1) 
                fs_criar_diretorio(args[1]);
            else 
                printf("Uso: mkdir <nomedodiretorio>\n");
        } else if (strcmp(args[0], "ls") == 0) {
            fs_listar();
        } else if (strcmp(args[0], "touch") == 0) {
            if (nargs > 1) 
                fs_criar_arquivo(args[1]);
            else 
                printf("Uso: touch <nomedoarquivo>\n");
        } else if (strcmp(args[0], "stat") == 0) {
            fs_estado();
        } else if (strcmp(args[0], "cd") == 0) {
            if (nargs > 1) 
                fs_mudar_diretorio(args[1]);
            else 
                printf("Uso: cd <caminho>\n");
        } else if (strcmp(args[0], "pwd") == 0) {
            fs_mostrar_caminho();
        } else if (strcmp(args[0], "cat") == 0) {
            if (nargs > 1) 
                fs_mostrar_arquivo(args[1]);
            else 
                printf("Uso: cat <nomedoarquivo>\n");
                
        } else if (strcmp(args[0], "rm") == 0) {
            if (nargs > 1) fs_remover(args[1]);
            else printf("Uso: rm <nome>\n");
        } else if (strcmp(args[0], "clear") == 0 || strcmp(args[0], "cls") == 0) {
            system("cls");
        }
        else {
            printf("Comando desconhecido: %s\n", args[0]);
        }
        
        // fs_desmontar();
        // fs_montar();
    }

    fs_desmontar();
}