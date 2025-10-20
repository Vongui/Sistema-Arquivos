Relatório do Simulador de Sistema de Arquivos (FS)

1. Introdução

Este documento descreve a arquitetura e as técnicas de implementação de um simulador de sistema de arquivos (FS) desenvolvido em C. O objetivo deste projeto é simular as operações fundamentais de um sistema de arquivos baseado em blocos, incluindo a formatação, montagem, criação de arquivos/diretórios e gerenciamento de espaço em disco.

A simulação não opera em uma partição de disco real, mas sim emula o "disco" utilizando arquivos e diretórios no sistema de arquivos do sistema operacional hospedeiro.

2. Declarações e Estruturas de Dados (fs.h)

O coração do sistema de arquivos é definido por suas estruturas de dados e constantes globais, declaradas em fs.h.

2.1. Constantes Principais

As constantes definem os parâmetros físicos do nosso disco simulado:
C

#define FILESYSTEM_NAME "fs"  // Nome do sistema de arquivos
#define BLOCK_SIZE 128          // Tamanho de cada bloco em bytes
#define PARTITION_SIZE 10240    // Tamanho total da partição (10 KB)
#define NUM_BLOCKS (PARTITION_SIZE / BLOCK_SIZE) // Total de blocos (80)
#define NUM_INODES NUM_BLOCKS   // Número de i-nodes (designado como 80)
#define MAX_FILENAME 14         // Tamanho máximo do nome de um arquivo
#define NUM_DIRECT_POINTERS 8   // Número de ponteiros diretos em um i-node

Com BLOCK_SIZE de 128 bytes e PARTITION_SIZE de 10240 bytes, o sistema de arquivos gerencia um total de 80 blocos (numerados de 0 a 79).

2.2. Estrutura Superbloco

O superbloco armazena os metadados globais sobre o sistema de arquivos. É a primeira estrutura lida durante a montagem para entender a geometria do "disco".
C

typedef struct {
    char filesystem[14];   // Nome do FS
    int blocksize;         // Tamanho do bloco (128)
    int partitionsize;     // Tamanho da partição (10240)
    int num_blocks;        // Número de blocos (80)
    int num_inodes;        // Número de i-nodes (80)
} Superbloco;

2.3. Estrutura Inode

O "index node" (i-node) é a estrutura fundamental de metadados para cada arquivo e diretório. Ele não armazena o nome do arquivo, mas sim todas as outras informações sobre ele.
C

typedef struct {
    char type; // 'f' (file), 'd' (diretorio), '0' (free)
    int size;  // Tamanho do arquivo em bytes
    int direct_pointers[NUM_DIRECT_POINTERS]; // Ponteiros diretos para blocos de dados
} Inode;

    type: Identifica se o i-node está livre ('0') ou se representa um arquivo ('f') ou diretório ('d').

    size: Para arquivos, é o tamanho em bytes. Para diretórios, é o tamanho total de suas EntradaDiretorio.

    direct_pointers: Um array fixo de 8 inteiros. Cada inteiro é o número de um bloco de dados que armazena o conteúdo do arquivo.

2.4. Estrutura EntradaDiretorio

Um diretório, em si, é um arquivo especial (type = 'd'). Seu conteúdo (armazenado nos blocos de dados apontados por seu i-node) é simplesmente uma lista de estruturas EntradaDiretorio. Esta estrutura cria a ligação entre um nome de arquivo legível e seu respectivo número de i-node.
C

typedef struct {
    char name[MAX_FILENAME]; // Nome do arquivo/diretório
    int inode_number;        // O número do i-node que descreve este arquivo
} EntradaDiretorio;

3. Técnicas de Simulação e Persistência

Como o simulador não tem acesso direto a um dispositivo de bloco, ele emula a persistência do disco da seguinte maneira:

    Emulação de Disco (Host OS): O "disco" é um diretório chamado fs/ no sistema hospedeiro.

    Persistência de Metadados:

        fs/Superbloco.dat: Um arquivo que armazena a única instância da struct Superbloco.

        fs/inodes.dat: Um arquivo que armazena o array completo de Inode[NUM_INODES].

        fs/freespace.dat: Um arquivo que armazena o bitmap de espaço livre.

    Persistência de Blocos de Dados:

        fs/blocks/: Um subdiretório que contém os blocos de dados.

        fs/blocks/N.dat: Cada bloco de dados (de 0.dat a 79.dat) é simulado como um arquivo individual de 128 bytes no sistema hospedeiro.

    Funções de "Driver de Disco": As funções escrever_bloco() e ler_bloco() abstraem a leitura e escrita no "disco". Elas constroem o caminho do arquivo (ex: "fs/blocks/5.dat") e usam fwrite() e fread() para simular operações de E/S de bloco.

4. Técnicas de Gerenciamento de Disco

4.1. Gerenciamento de Espaço Livre (Bitmap)

Para rastrear quais dos 80 blocos de dados estão em uso ou livres, foi implementado um bitmap de espaço livre.

    Estrutura: unsigned char mapa_espaco_livre[NUM_BLOCKS / 8];

    Tamanho: Com 80 blocos, o mapa é um array de 10 bytes (80 blocos / 8 bits por byte).

    Técnica: Cada bit neste array corresponde a um bloco de dados.

        Bit = 0: Bloco está livre.

        Bit = 1: Bloco está em uso.

    Operações: As funções definir_bit(), limpar_bit() e obter_bit() usam operações bitwise (como |= e &= ~) e máscaras de bits (como 1 << (num_bloco % 8)) para manipular com eficiência o bit exato correspondente a um número de bloco.

    Alocação: encontrar_bloco_livre() implementa uma política de first-fit, varrendo o bitmap linearmente até encontrar o primeiro bit '0'.

4.2. Gerenciamento de I-nodes (Lista Fixa)

O gerenciamento de i-nodes é feito por um array fixo em fs/inodes.dat.

    Estrutura: Inode inodes[NUM_INODES];

    Técnica: Um i-node é considerado "livre" se seu campo type for igual a '0'.

    Alocação: A função encontrar_inode_livre() implementa uma política de first-fit, varrendo o array inodes do início até encontrar a primeira entrada com type == '0'.

5. Técnicas de Alocação de Arquivos

5.1. Alocação Direta

Este FS implementa uma forma simples de alocação indexada: Alocação Direta.

    Técnica: O Inode possui um array fixo direct_pointers[8].

    Implementação: Quando um arquivo é criado (fs_criar_arquivo), os dados do usuário são divididos em "chunks" de 128 bytes (o BLOCK_SIZE).

    Para cada chunk, o sistema solicita um bloco livre (encontrar_bloco_livre()), armazena o número desse bloco no próximo índice disponível do direct_pointers (ex: direct_pointers[0], direct_pointers[1], etc.), e escreve o chunk de dados nesse bloco.

    Limitação: Esta técnica limita o tamanho máximo de um arquivo a NUM_DIRECT_POINTERS * BLOCK_SIZE, que neste sistema é 8 * 128 = 1024 bytes (1 KB). Não há implementação de ponteiros indiretos.

5.2. Estrutura de Diretórios

    Técnica: Um diretório é um arquivo (type = 'd') cujo conteúdo é um array de EntradaDiretorio.

    Implementação: Na fs_criar_diretorio, o bloco de dados do diretório pai é lido. Uma nova EntradaDiretorio (com o nome do novo diretório e seu número de i-node) é anexada a esse bloco de dados, e o size do i-node pai é incrementado.

    Simplificação: A implementação atual assume que o conteúdo de um diretório (sua lista de entradas) caberá em um único bloco de dados (o apontado por direct_pointers[0]). Isso limita o número de arquivos/subdiretórios que podem existir dentro de um mesmo diretório (aprox. 128 / sizeof(EntradaDiretorio) entradas).

6. Navegação e Hierarquia

A estrutura de árvore do sistema de arquivos é mantida usando duas entradas especiais em cada diretório: . e ...

    Técnica:

        . ("ponto"): É uma EntradaDiretorio que aponta para o i-node do próprio diretório.

        .. ("ponto-ponto"): É uma EntradaDiretorio que aponta para o i-node do diretório pai.

    Exceção (Raiz): No diretório raiz (criado durante a formatação, inode 0), tanto . quanto .. apontam para si mesmo (inode 0).

    Navegação (fs_mudar_diretorio): O comando cd funciona procurando na lista de EntradaDiretorio do diretório atual. Se uma entrada com o nome correspondente e type = 'd' for encontrada, a variável global inode_diretorio_atual é atualizada para o i-node daquela entrada.

    Caminho Atual (fs_obter_caminho_atual): O comando pwd implementa uma lógica de subida na árvore. Ele começa no inode_diretorio_atual, lê sua entrada .. para encontrar o i-node pai. Em seguida, ele lê o bloco de dados do pai para encontrar qual entrada aponta para o i-node atual, descobrindo assim seu próprio nome. Esse processo é repetido até que o i-node pai seja 0 (a raiz). Os nomes são então montados na ordem inversa para exibir o caminho completo.