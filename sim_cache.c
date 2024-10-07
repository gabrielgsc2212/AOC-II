/*
SIMULADOR DE MEMÓRIA CACHE PARA TRABALHO DE AOCII -2024.1
TURMA M1: GABRIEL GONZAGA SEABRA CÂMARA;
COMPILAR COM: gcc sim_cache.c -o cache.exe -lm -Wall
EXECUTAR COM: ./cache.exe <nsets> <bsize> <assoc> <substituição> <flag_saida> <arquivo_de_entrada.bin>
*/

/* O único erro presente no código é referente é o defeito na captação de misses de conflito */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <limits.h>

// Representação da memória cache
typedef struct cachetable {
    int tag_cache;
    int validity_cache;
    int lru_counter;
    int fifo_position;
} cachetable;

void substituir_bloco(cachetable *conjunto, int associativity, int tag, const char *replacement, int *previous_use_found, int *fifo_counter, int *global_time_counter) {
    *previous_use_found = 0;

    if (strcmp(replacement, "R") == 0 || strcmp(replacement, "r") == 0) {
        // Substituição RANDOM: escolhe um bloco aleatório para substituição
        int rand_idx = rand() % associativity;
        if (conjunto[rand_idx].validity_cache == 1) {
            *previous_use_found = 1; // O bloco já estava sendo utilizado
        }
        conjunto[rand_idx].tag_cache = tag;
        conjunto[rand_idx].validity_cache = 1;
        conjunto[rand_idx].lru_counter = (*global_time_counter)++;  // Atualiza o contador de LRU
        conjunto[rand_idx].fifo_position = (*fifo_counter)++;       // Atualiza o contador de FIFO
    } else if (strcmp(replacement, "L") == 0 || strcmp(replacement, "l") == 0) {
        // Substituição LRU: substitui o bloco menos recentemente utilizado
        int least_recently_used = INT_MAX, lru_idx = 0;
        for (int i = 0; i < associativity; i++) {
            if (conjunto[i].lru_counter < least_recently_used) {
                least_recently_used = conjunto[i].lru_counter;
                lru_idx = i;
            }
        }
        if (conjunto[lru_idx].validity_cache == 1) {
            *previous_use_found = 1; // O bloco já estava sendo utilizado
        }
        conjunto[lru_idx].tag_cache = tag;
        conjunto[lru_idx].validity_cache = 1;
        conjunto[lru_idx].lru_counter = (*global_time_counter)++;  // Atualiza o tempo de uso do bloco
        conjunto[lru_idx].fifo_position = (*fifo_counter)++;       // Atualiza o contador de FIFO
    } else if (strcmp(replacement, "F") == 0 || strcmp(replacement, "f") == 0) {
        // Substituição FIFO: substitui o bloco mais antigo (primeiro a entrar)
        int oldest_position = INT_MAX, fifo_idx = 0;
        for (int i = 0; i < associativity; i++) {
            if (conjunto[i].fifo_position < oldest_position) {
                oldest_position = conjunto[i].fifo_position;
                fifo_idx = i;
            }
        }
        if (conjunto[fifo_idx].validity_cache == 1) {
            *previous_use_found = 1; // O bloco já estava sendo utilizado
        }
        conjunto[fifo_idx].tag_cache = tag;
        conjunto[fifo_idx].validity_cache = 1;
        conjunto[fifo_idx].lru_counter = (*global_time_counter)++; // Atualiza o contador de tempo
        conjunto[fifo_idx].fifo_position = (*fifo_counter)++;      // Atualiza a posição FIFO
    }
}

int main(int argc, char const *argv[]) {
    // Verifica se os argumentos foram passados corretamente
    if (argc != 7) {
        printf("Número de argumentos incorreto. Utilize:\n");
        printf("./cache_simulator <nsets> <bsize> <assoc> <substituição> <flag_saida> <arquivo_de_entrada.bin>\n");
        return EXIT_FAILURE;
    }

    srand(time(NULL));

    // Variáveis principais do simulador
    int nsets = atoi(argv[1]), bsize = atoi(argv[2]), associativity = atoi(argv[3]), flag = atoi(argv[5]);
    int n_bits_index = 0, n_bits_offset = 0, index_read = 0, tag_read = 0;
    int total_addresses = 0, fifo_counter = 0, global_time_counter = 0;  // Contadores globais para FIFO e LRU
    float hit = 0, miss = 0, miss_compulsory = 0, miss_capacity = 0, miss_conflict = 0;  // Estatísticas
    char replacement[3];
    unsigned int address;

    // Abre o arquivo binário de entrada
    FILE *input_file = fopen(argv[6], "rb");
    if (input_file == NULL) {
        printf("Erro ao abrir o arquivo de entrada. Verifique o caminho e tente novamente.\n");
        return EXIT_FAILURE;
    }

    // Cálculo dos bits necessários para index e offset
    n_bits_index = log2(nsets);
    n_bits_offset = log2(bsize);

    // Inicialização da memória cache
    cachetable **cachememory = (cachetable **)malloc(nsets * sizeof(cachetable *));
    for (int i = 0; i < nsets; i++) {
        cachememory[i] = (cachetable *)malloc(associativity * sizeof(cachetable));
        for (int j = 0; j < associativity; j++) {
            cachememory[i][j].tag_cache = 0;
            cachememory[i][j].validity_cache = 0;
            cachememory[i][j].lru_counter = 0;  // Inicializa o contador LRU
            cachememory[i][j].fifo_position = 0; // Inicializa o contador FIFO
        }
    }

    // Define o algoritmo de substituição (R, L, F)
    strcpy(replacement, argv[4]);

    // Leitura e processamento de endereços do arquivo de entrada
    while (fread(&address, sizeof(int), 1, input_file) == 1) {
        address = __builtin_bswap32(address);  // Converte o endereço de big-endian para little-endian
        index_read = (address >> n_bits_offset) & ((int)pow(2, n_bits_index) - 1);  // Extrai o índice da cache
        tag_read = address >> (n_bits_offset + n_bits_index);  // Extrai o tag
        total_addresses++;

        // Verifica se há um hit
        int hit_occurred = 0;
        int free_slot_found = 0;
        int previous_use_found = 0;

        // Verifica se o bloco já está na cache (hit)
        for (int i = 0; i < associativity; i++) {
            if (cachememory[index_read][i].tag_cache == tag_read && cachememory[index_read][i].validity_cache == 1) {
                hit++;
                cachememory[index_read][i].lru_counter = global_time_counter++;  // Atualiza o tempo de uso no acesso
                hit_occurred = 1;
                break;
            }
        }

        // Se não houver hit, processa o miss
        if (!hit_occurred) {
            miss++;

            // Verifica se há um slot livre (miss compulsório)
            for (int i = 0; i < associativity; i++) {
                if (cachememory[index_read][i].validity_cache == 0) {
                    miss_compulsory++;
                    cachememory[index_read][i].tag_cache = tag_read;
                    cachememory[index_read][i].validity_cache = 1;
                    cachememory[index_read][i].lru_counter = global_time_counter++;
                    cachememory[index_read][i].fifo_position = fifo_counter++;
                    free_slot_found = 1;
                    break;
                }
            }

            // Se não houver slot livre, aplica a política de substituição
            if (!free_slot_found) {
                substituir_bloco(cachememory[index_read], associativity, tag_read, replacement, &previous_use_found, &fifo_counter, &global_time_counter);

                // Diferencia os tipos de miss (capacidade ou conflito)
                if (previous_use_found) {
                    miss_capacity++;
                } else {
                    miss_conflict++;
                }
            }
        }
    }

    float hit_rate = (hit / total_addresses);
    float miss_rate = (miss / total_addresses);
    float miss_compulsory_rate = (miss_compulsory / miss);
    float miss_capacity_rate = (miss_capacity / miss);
    float miss_conflict_rate = (miss_conflict / miss);

    if (flag == 0) {
        printf("Total de acessos: %d\n", total_addresses);
        printf("Hits: %.4f\n", hit_rate);
        printf("Misses: %.4f\n", miss_rate);
        printf("Misses compulsórios: %.4f%%\n", miss_compulsory_rate);
        printf("Misses por capacidade: %.4f%%\n", miss_capacity_rate);
        printf("Misses por conflito: %.4f%%\n", miss_conflict_rate);
    } else if (flag == 1) {
        printf("%d  %.4f  %.4f  %.4f  %.4f  %.4f\n", total_addresses, hit_rate, miss_rate, miss_compulsory_rate, miss_capacity_rate, miss_conflict_rate);
    } else {
        printf("Flag de saída inválida. Utilize 0 ou 1.\n");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < nsets; i++) {
        free(cachememory[i]);
    }
    free(cachememory);
    fclose(input_file);

    return 0;
}