## Chat Server in Netwide Assembly

A simple chat server and client implemented entirely in Assembly for x86-64 Linux.

Chat means cat in French, hence the project name Miaou echoing the sound a cat makes in French.

### Prerequisites

* NASM assembler
* GNU `ld` linker
* Make

### Directory layout

```
inc/malloc/     Custom malloc implementation
inc/utils/      Utility routines
inc/boeuf/      Dynamic buffers library
inc/miaou/      Chat protocol definitions
src/        Assembly sources (.s)
build/      Output directory for executables
```

### Build

Use the Makefile to assemble and link executables. Specify the target via `PROGRAM_NAME`.

* Build the **client**:

  ```sh
  make PROGRAM_NAME=client
  ```
* Build the **server**:

  ```sh
  make PROGRAM_NAME=server
  ```

Executables are placed in the `build/` directory:

```
build/client
build/server
```

### Run

1. Start the server in one terminal:

   ```sh
   ./build/server
   ```
2. In another terminal, start a client:

   ```sh
   ./build/client
   ```
3. Multiple clients can connect to the server to chat.

