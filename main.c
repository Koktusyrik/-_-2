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

    /* Precomputed indices of letters in each decimal column (-1 = no digit). */
    signed char column_letter[MAX_WORD_LENGTH][MAX_WORDS];
    int max_length;
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

    /* Machine-independent optimization: perform indexing and position
       calculations once, rather than at every recursive search step. */
    memset(puzzle->column_letter, -1, sizeof(puzzle->column_letter));
    for (int w = 0; w <= puzzle->addend_count; ++w) {
        if (puzzle->lengths[w] > puzzle->max_length) {
            puzzle->max_length = puzzle->lengths[w];
        }
        for (int col = 0; col < puzzle->lengths[w]; ++col) {
            puzzle->column_letter[col][w] =
                (signed char)(puzzle->words[w][puzzle->lengths[w] - col - 1] - 'A');
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

/* Assign digits column by column, from right to left.
   sum starts with the carry from the previous column. */
static int brute_force(Puzzle *puzzle, int column, int word_index,
                       int sum, int max_length) {
    ++recursive_calls;

    if (column == max_length) {
        /* All columns have been checked; no carry may remain. */
        if (sum != 0) {
            return 0;
        }
        ++checked_combinations;
        return is_correct_solution(puzzle);
    }

    if (word_index < puzzle->addend_count) {
        int letter = puzzle->column_letter[column][word_index];
        if (letter < 0) {
            return brute_force(puzzle, column, word_index + 1, sum, max_length);
        }

        int assigned_digit = puzzle->digit_for_letter[letter];
        if (assigned_digit >= 0) {
            return brute_force(puzzle, column, word_index + 1,
                               sum + assigned_digit, max_length);
        }

        /* Heuristic (forward checking): if the result digit is already known
           and all other addend digits in this column are assigned, the last
           unknown digit is determined modulo 10.  A repeated occurrence of
           the unknown letter is excluded from this shortcut. */
        int forced_digit = -1;
        int result_letter = puzzle->column_letter[column][puzzle->addend_count];
        if (result_letter >= 0) {
            int known_result = puzzle->digit_for_letter[result_letter];
            if (known_result >= 0) {
                int remaining_sum = 0;
                int all_other_known = 1;
                for (int w = word_index + 1; w < puzzle->addend_count; ++w) {
                    int next_letter = puzzle->column_letter[column][w];
                    if (next_letter < 0) {
                        continue;
                    }
                    if (next_letter == letter) {
                        all_other_known = 0;  /* coefficient would exceed 1 */
                        break;
                    }
                    int value = puzzle->digit_for_letter[next_letter];
                    if (value < 0) {
                        all_other_known = 0;
                        break;
                    }
                    remaining_sum += value;
                }
                if (all_other_known) {
                    forced_digit = (known_result - sum - remaining_sum) % 10;
                    if (forced_digit < 0) {
                        forced_digit += 10;
                    }
                }
            }
        }

        for (int digit = 0; digit <= 9; ++digit) {
            if ((forced_digit >= 0 && digit != forced_digit) ||
                puzzle->digit_used[digit] ||
                (digit == 0 && puzzle->cannot_be_zero[letter])) {
                continue;
            }

            puzzle->digit_for_letter[letter] = digit;
            puzzle->digit_used[digit] = 1;

            if (brute_force(puzzle, column, word_index + 1,
                            sum + digit, max_length)) {
                return 1;
            }

            puzzle->digit_used[digit] = 0;
            puzzle->digit_for_letter[letter] = -1;
        }
        return 0;
    }

    /* Every addend digit in this column is now known. */
    int expected_digit = sum % 10;
    int next_carry = sum / 10;
    int result_letter = puzzle->column_letter[column][puzzle->addend_count];

    if (result_letter < 0) {
        if (expected_digit != 0) {
            return 0;
        }
        return brute_force(puzzle, column + 1, 0, next_carry, max_length);
    }

    int letter = result_letter;
    int assigned_digit = puzzle->digit_for_letter[letter];
    if (assigned_digit >= 0) {
        if (assigned_digit != expected_digit) {
            return 0;
        }
        return brute_force(puzzle, column + 1, 0, next_carry, max_length);
    }

    if (puzzle->digit_used[expected_digit] ||
        (expected_digit == 0 && puzzle->cannot_be_zero[letter])) {
        return 0;
    }

    /* The result digit is determined by the current column. */
    puzzle->digit_for_letter[letter] = expected_digit;
    puzzle->digit_used[expected_digit] = 1;

    if (brute_force(puzzle, column + 1, 0, next_carry, max_length)) {
        return 1;
    }

    puzzle->digit_used[expected_digit] = 0;
    puzzle->digit_for_letter[letter] = -1;
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

    /* Heuristic: with exactly two addends, an extra leading result
       column always contains the carry 1. Assign it before the search. */
    if (puzzle.addend_count == 2) {
        int max_addend_length = puzzle.lengths[0];
        if (puzzle.lengths[1] > max_addend_length) {
            max_addend_length = puzzle.lengths[1];
        }
        if (puzzle.lengths[2] == max_addend_length + 1) {
            int leading = puzzle.words[2][0] - 'A';
            puzzle.digit_for_letter[leading] = 1;
            puzzle.digit_used[1] = 1;
        }
    }

    if (!brute_force(&puzzle, 0, 0, 0, puzzle.max_length)) {
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
