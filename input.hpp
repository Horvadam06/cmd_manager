#ifndef INPUT_H
#define INPUT_H

    enum class Key {
        Character, Tab, Backspace, Delete,
        ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
        Enter, Unknown
    };

    struct KeyPress {
        Key key;
        char ch = 0;
    };

    void enableRawMode();
    void disableRawMode();
    KeyPress readKey();
#endif