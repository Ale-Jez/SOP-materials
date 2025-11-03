### Standard Library Functions

* **`FILE`**
    A type definition (structure) used to store information about a file stream, such as its buffer, position, and error status. `FILE*` is a pointer to this structure.
* **`getline(char **lineptr, size_t *n, FILE *stream)`**
    Reads an entire line from `stream`, storing the text in a buffer pointed to by `*lineptr`. It automatically allocates or reallocates memory for the buffer as needed, updating `*lineptr` and the buffer size `*n`. Returns -1 on failure or end-of-file.
* **`strlen(const char *str)`**
    Calculates the length of the string `str`, excluding the terminating null character (`\0`).
* **`strtok(char *str, const char *delim)`**
    Breaks the string `str` into a series of tokens using characters in `delim`. The first call requires `str`; subsequent calls to continue tokenizing the same string must use `NULL` as the first argument. This function modifies the source string by placing null characters.
* **`strcmp(const char *s1, const char *s2)`**
    Compares the two strings `s1` and `s2` lexicographically. Returns 0 if the strings are identical.
* **`strdup(const char *s)`**
    Duplicates the string `s`. It allocates new memory using `malloc` for the copy and returns a pointer to it. This memory must be released using `free()`. (Note: `strdup` is POSIX, not standard C, but widely available).
* **`free(void *ptr)`**
    Deallocates a block of memory previously allocated by `malloc`, `calloc`, `realloc`, or (in this case) `getline` and `strdup`.
* **`printf(const char *format, ...)`**
    Prints formatted data to standard output (stdout) according to the `format` string.


    --------

`strtok` is a stateful, destructive function.

1.  **Stateful (Internal Pointer):** `strtok` uses a hidden, **static pointer** internally.

      * **First Call:** When you pass your string (`str`) to the first call, `strtok` saves a pointer to this string inside itself.
      * **Subsequent Calls:** When you pass `NULL`, you are signaling `strtok` to "resume from where you left off" using its internal saved pointer.
      * This internal static pointer is why `strtok` is **not thread-safe**. If two threads call `strtok` simultaneously on different strings, they will overwrite each other's saved pointer, leading to corrupt data.

2.  **Destructive (String Modification):** `strtok` **modifies the original string** you pass to it. It does not create new strings or tokens.

      * It finds the first delimiter character.
      * It replaces that delimiter character in your original string with a null-terminator (`\0`).
      * It returns a pointer to the beginning of the token (the start of the substring).

-----

### Example Walkthrough

Consider this code:

```c
char str[] = "author:title";
char* token;

// First call
token = strtok(str, ":");
// Second call
token = strtok(NULL, ":");
```

**Before any calls:**

  * `str` points to memory: `['a']['u']['t']['h']['o']['r'][':']['t']['i']['t']['l']['e']['\0']`

**1. First Call: `token = strtok(str, ":");`**

  * `strtok` is given the address of `str`. It saves this address internally.
  * It scans `str` from the beginning.
  * It finds the delimiter `':'` at index 6.
  * It **overwrites** this `':'` with a null terminator `\0`.
  * The memory `str` points to is now: `['a']['u']['t']['h']['o']['r']['\0']['t']['i']['t']['l']['e']['\0']`
  * `strtok` returns a pointer to the beginning of the token it found, which is the address of `str[0]` (pointing to 'a').
  * `token` now points to the null-terminated string "author".

**2. Second Call: `token = strtok(NULL, ":");`**

  * `strtok` sees the `NULL` argument and uses its internal pointer, which points to the character *after* the `\0` it just wrote (i.e., at `str[7]`, the 't' in "title").
  * It scans from this new position.
  * It reaches the end of the string (`\0`) without finding another delimiter.
  * It returns a pointer to the beginning of this second token (`str[7]`).
  * `token` now points to the null-terminated string "title".

**3. Third Call: `token = strtok(NULL, ":");`**

  * `strtok` sees `NULL` and resumes from its internal pointer.
  * It is already at the end of the string.
  * It finds no more tokens.
  * It returns `NULL`.

Because `strtok` modifies the string, you cannot use it on string literals (e.g., `const char* s = "author:title";`), as attempting to write a `\0` into read-only memory will cause a crash.

-----

This video tutorial explains the `strtok` function in C, demonstrating how it works.
[C Programming Tutorial | strtok() function](https://www.youtube.com/watch?v=nrO_pXGZc3Y)
http://googleusercontent.com/youtube_content/0