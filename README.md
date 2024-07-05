# Atom Text Editor Usage Documentation

This text editor is developed based on the concepts and functionalities of Vim, known for its efficiency and powerful text editing capabilities.

## Installation and Launch

1. **Download and Compile the Source Code**:
   - Ensure that all necessary libraries and dependencies are installed.
   - Compile the source code using a C++ compiler.

2. **Starting the Editor**:
   - Launch the compiled application.
   - Enter the filename you wish to open or create. If the file exists, its content will be loaded into the editor.

## Modes of Operation

The editor supports three modes of operation:

- **Normal Mode**: The primary mode for navigating through text and executing commands.
- **Insert Mode**: Mode for entering and editing text.
- **Command Mode**: Mode for executing commands such as saving files, exiting the editor, and other operations.

## Key Commands

### Navigation

- `h`, `j`, `k`, `l`: Move the cursor left, down, up, and right respectively.
- `w`: Move to the beginning of the next word.
- `b`: Move to the beginning of the previous word.

### Editing

- `i`: Enter insert mode (for typing and editing text).
- `dd`: Delete the current line.
- `dw`: Delete from the current position to the beginning of the next word.
- `yy`: Yank (copy) the current line.
- `p`: Paste the contents of the buffer after the current line.
- `P`: Paste the contents of the buffer before the current line.

### Undo and Redo

- `u`: Undo the last action.
- `Ctrl+R`: Redo the last undone action.

### Saving and Exiting

- `:w`: Save changes to the file.
- `:q`: Exit the editor (if there are unsaved changes, the editor will prompt to save).

### Searching and Replacing

- `/`: Start a search. Enter the search string and press Enter to find occurrences in the text.
- `:s/old/new/`: Replace all occurrences of `old` with `new` in the current buffer.
