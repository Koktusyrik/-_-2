#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_SLAGAEMIH 7
#define MAX_WORDS (MAX_SLAGAEMIH + 1)
#define MAX_WORD_LENGTH 100
#define MAX_INPUT_LENGTH 2048
#define MAX_LETTERS 10

typedef struct {
    char words[MAX_WORDS][MAX_WORD_LENGTH + 1];
    int lengths[MAX_WORDS];
    int addend_count;

    char letters[MAX_LETTERS];
    int letter_count;
    int digit_for_letter[26];
    int digit_used[10];
    int cannot_be_zero[26];
} Puzzle;

/* Measurement counters; they do not change the search algorithm. */
static unsigned long long recursive_calls = 0;
static unsigned long long checked_combinations = 0;

/* Read the puzzle and collect its distinct letters. */
static int razbor_rebusa(const char *input, Puzzle *puzzle) {
    memset(puzzle, 0, sizeof(*puzzle));
    for (int i = 0; i < 26; ++i) {
        puzzle->digit_for_letter[i] = -1;
    }

    int word_index = 0;
    int after_equals = 0;
    int need_letter = 1;

    for (const unsigned char *c = (const unsigned char *)input; *c != '\0'; ++c) {
        if (isspace(*c)) {
            continue;
        }

        if (*c >= 'A' && *c <= 'Z') {
            if (word_index >= MAX_WORDS ||
                puzzle->lengths[word_index] >= MAX_WORD_LENGTH) {
                return 0;
            }

            int pos = puzzle->lengths[word_index]++;
            puzzle->words[word_index][pos] = (char)*c;
            need_letter = 0;
        } else if (*c == '+' && !after_equals) {
            if (need_letter || word_index >= MAX_SLAGAEMIH - 1) {
                return 0;
            }
            ++word_index;
            need_letter = 1;
        } else if (*c == '=' && !after_equals) {
            if (need_letter || word_index < 1 || word_index >= MAX_SLAGAEMIH) {
                return 0;
            }
            puzzle->addend_count = word_index + 1;
            ++word_index;
            after_equals = 1;
            need_letter = 1;
        } else {
            return 0;
        }
    }

    if (!after_equals || need_letter || word_index != puzzle->addend_count) {
        return 0;
    }

    int seen[26] = {0};
    for (int w = 0; w <= puzzle->addend_count; ++w) {
        if (puzzle->lengths[w] > 1) {
            puzzle->cannot_be_zero[puzzle->words[w][0] - 'A'] = 1;
        }
        for (int i = 0; i < puzzle->lengths[w]; ++i) {
            int letter = puzzle->words[w][i] - 'A';
            if (!seen[letter]) {
                if (puzzle->letter_count == MAX_LETTERS) {
                    return 0;
                }
                seen[letter] = 1;
                puzzle->letters[puzzle->letter_count++] = (char)('A' + letter);
            }
        }
    }

    return 1;
}

/* Check a complete digit assignment, without integer overflow. */
static int is_correct_solution(const Puzzle *puzzle) {
    for (int w = 0; w <= puzzle->addend_count; ++w) {
        if (puzzle->lengths[w] > 1 &&
            puzzle->digit_for_letter[puzzle->words[w][0] - 'A'] == 0) {
            return 0;
        }
    }

    int max_length = 0;
    for (int w = 0; w <= puzzle->addend_count; ++w) {
        if (puzzle->lengths[w] > max_length) {
            max_length = puzzle->lengths[w];
        }
    }

    int carry = 0;
    for (int column = 0; column < max_length; ++column) {
        int sum = carry;

        for (int w = 0; w < puzzle->addend_count; ++w) {
            int pos = puzzle->lengths[w] - 1 - column;
            if (pos >= 0) {
                sum += puzzle->digit_for_letter[puzzle->words[w][pos] - 'A'];
            }
        }

        int result_pos = puzzle->lengths[puzzle->addend_count] - 1 - column;
        int result_digit = 0;
        if (result_pos >= 0) {
            result_digit = puzzle->digit_for_letter[
                puzzle->words[puzzle->addend_count][result_pos] - 'A'
            ];
        }

        if (sum % 10 != result_digit) {
            return 0;
        }
        carry = sum / 10;
    }

    return carry == 0;
}

/* Basic brute force: assign digits to all letters, then check the sum. */
static int brute_force(Puzzle *puzzle, int index) {
    ++recursive_calls;
    if (index == puzzle->letter_count) {
        ++checked_combinations;
        return is_correct_solution(puzzle);
    }

    int letter = puzzle->letters[index] - 'A';
    for (int digit = 0; digit <= 9; ++digit) {
        if (puzzle->digit_used[digit] ||
            (digit == 0 && puzzle->cannot_be_zero[letter])) {
            continue;
        }

        puzzle->digit_for_letter[letter] = digit;
        puzzle->digit_used[digit] = 1;

        if (brute_force(puzzle, index + 1)) {
            return 1;
        }

        puzzle->digit_used[digit] = 0;
        puzzle->digit_for_letter[letter] = -1;
    }

    return 0;
}

/* Convert the found assignment to an arithmetic result string. */
static char *make_answer(const Puzzle *puzzle) {
    size_t text_length = 3u * (size_t)puzzle->addend_count + 1u;
    for (int w = 0; w <= puzzle->addend_count; ++w) {
        text_length += (size_t)puzzle->lengths[w];
    }

    char *answer = malloc(text_length);
    if (answer == NULL) {
        return NULL;
    }

    char *out = answer;
    for (int w = 0; w <= puzzle->addend_count; ++w) {
        if (w > 0) {
            const char *separator = (w == puzzle->addend_count) ? " = " : " + ";
            memcpy(out, separator, 3);
            out += 3;
        }

        for (int i = 0; i < puzzle->lengths[w]; ++i) {
            int letter = puzzle->words[w][i] - 'A';
            *out++ = (char)('0' + puzzle->digit_for_letter[letter]);
        }
    }
    *out = '\0';
    return answer;
}

/* Returns a dynamically allocated answer or NULL if solving fails. */
char *solve_puzzle(const char *result) {
    if (result == NULL) {
        return NULL;
    }

    Puzzle puzzle;
    if (!razbor_rebusa(result, &puzzle)) {
        return NULL;
    }

    if (!brute_force(&puzzle, 0)) {
        return NULL;
    }

    return make_answer(&puzzle);
}

int main(void) {
    char result[MAX_INPUT_LENGTH];

    printf("Enter a puzzle (e.g. SEND + MORE = MONEY):\n");
    if (fgets(result, sizeof(result), stdin) == NULL) {
        fprintf(stderr, "Input error.\n");
        return 1;
    }

    /* Repeat fast runs until the total measured CPU time is large enough.
       Search logic is unchanged; counters refer to one complete run. */
    char *answer = NULL;
    int repetitions = 0;
    clock_t start_time = clock();

    do {
        recursive_calls = 0;
        checked_combinations = 0;

        char *current_answer = solve_puzzle(result);
        if (current_answer == NULL) {
            free(answer);
            printf("No solution found or invalid input.\n");
            return 1;
        }

        if (answer == NULL) {
            answer = current_answer;
        } else {
            free(current_answer);
        }
        ++repetitions;
    } while ((clock() - start_time) < CLOCKS_PER_SEC / 4 && repetitions < 100000);

    clock_t end_time = clock();
    double elapsed_ms = 1000.0 * (double)(end_time - start_time)
                        / CLOCKS_PER_SEC / repetitions;

    printf("Solution: %s\n", answer);
    printf("Average time: %.6f ms\n", elapsed_ms);
    printf("Repetitions: %d\n", repetitions);
    printf("Recursive calls: %llu\n", recursive_calls);
    printf("Checked combinations: %llu\n", checked_combinations);
    free(answer);
    return 0;
}
