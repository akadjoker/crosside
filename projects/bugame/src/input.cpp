#include "bindings.hpp"
#include "camera.hpp"
#include <raylib.h>
#include <vector>

extern CameraManager gCamera;


namespace BindingsInput
{
    struct VirtualKey
    {
        int keyCode;
        Rectangle bounds;
        bool down;
        bool pressed;
        bool released;
    };

    static std::vector<VirtualKey> s_virtualKeys;
    static std::vector<int> s_prevTouchIds;
    static std::vector<int> s_currTouchIds;
    static bool s_virtualKeysVisible = true;
    static bool s_anyTouchPressed = false;
    static bool s_anyTouchReleased = false;

    static bool containsId(const std::vector<int> &ids, int value)
    {
        for (size_t i = 0; i < ids.size(); i++)
        {
            if (ids[i] == value)
                return true;
        }
        return false;
    }

    static bool isVirtualKeyDownByCode(int keyCode)
    {
        for (size_t i = 0; i < s_virtualKeys.size(); i++)
        {
            if (s_virtualKeys[i].keyCode == keyCode && s_virtualKeys[i].down)
                return true;
        }
        return false;
    }

    static bool isVirtualKeyPressedByCode(int keyCode)
    {
        for (size_t i = 0; i < s_virtualKeys.size(); i++)
        {
            if (s_virtualKeys[i].keyCode == keyCode && s_virtualKeys[i].pressed)
                return true;
        }
        return false;
    }

    static bool isVirtualKeyReleasedByCode(int keyCode)
    {
        for (size_t i = 0; i < s_virtualKeys.size(); i++)
        {
            if (s_virtualKeys[i].keyCode == keyCode && s_virtualKeys[i].released)
                return true;
        }
        return false;
    }

    static Vector2 getTouchScreenPositionSafe(int index)
    {
        int count = GetTouchPointCount();
        if (index < 0 || index >= count)
            return Vector2{-1.0f, -1.0f};
        return GetTouchPosition(index);
    }

    static Vector2 getTouchWorldPositionSafe(int index)
    {
        Vector2 screenPos = getTouchScreenPositionSafe(index);
        if (screenPos.x < 0.0f || screenPos.y < 0.0f)
            return screenPos;
        return GetScreenToWorld2D(screenPos, gCamera.getCamera());
    }

    static bool isVirtualKeyTouched(const VirtualKey &vk)
    {
        int count = GetTouchPointCount();
        for (int i = 0; i < count; i++)
        {
            Vector2 touch = GetTouchPosition(i);
            if (CheckCollisionPointRec(touch, vk.bounds))
                return true;
        }

        if (IsMouseButtonDown(0))
        {
            Vector2 mousePos = GetMousePosition();
            if (CheckCollisionPointRec(mousePos, vk.bounds))
                return true;
        }

        return false;
    }

    void update()
    {
        s_prevTouchIds = s_currTouchIds;
        s_currTouchIds.clear();

        int touchCount = GetTouchPointCount();
        for (int i = 0; i < touchCount; i++)
        {
            int id = GetTouchPointId(i);
            if (id >= 0)
                s_currTouchIds.push_back(id);
        }

        s_anyTouchPressed = false;
        s_anyTouchReleased = false;

        for (size_t i = 0; i < s_currTouchIds.size(); i++)
        {
            if (!containsId(s_prevTouchIds, s_currTouchIds[i]))
            {
                s_anyTouchPressed = true;
                break;
            }
        }

        for (size_t i = 0; i < s_prevTouchIds.size(); i++)
        {
            if (!containsId(s_currTouchIds, s_prevTouchIds[i]))
            {
                s_anyTouchReleased = true;
                break;
            }
        }

        for (size_t i = 0; i < s_virtualKeys.size(); i++)
        {
            VirtualKey &vk = s_virtualKeys[i];
            bool wasDown = vk.down;
            bool nowDown = isVirtualKeyTouched(vk);

            vk.down = nowDown;
            vk.pressed = (!wasDown && nowDown);
            vk.released = (wasDown && !nowDown);
        }
    }

    void drawVirtualKeys()
    {
        if (!s_virtualKeysVisible)
            return;

        for (size_t i = 0; i < s_virtualKeys.size(); i++)
        {
            const VirtualKey &vk = s_virtualKeys[i];
            Color fill = vk.down ? Color{255, 192, 64, 140} : Color{230, 230, 230, 80};
            Color border = vk.down ? Color{255, 220, 120, 220} : Color{255, 255, 255, 180};
            DrawRectangleRec(vk.bounds, fill);
            DrawRectangleLinesEx(vk.bounds, 2.0f, border);

            const char *label = TextFormat("%d", vk.keyCode);
            int fontSize = 20;
            int textWidth = MeasureText(label, fontSize);
            int tx = (int)(vk.bounds.x + (vk.bounds.width - textWidth) * 0.5f);
            int ty = (int)(vk.bounds.y + (vk.bounds.height - fontSize) * 0.5f);
            DrawText(label, tx, ty, fontSize, border);
        }
    }

    static Vector2 getMouseWorldPosition()
    {
        Vector2 screenPos = GetMousePosition();
        return GetScreenToWorld2D(screenPos, gCamera.getCamera());
    }


    //   RLAPI bool IsKeyPressed(int key);                             // Check if a key has been pressed once
    // RLAPI bool IsKeyPressedRepeat(int key);                       // Check if a key has been pressed again (Only PLATFORM_DESKTOP)
    // RLAPI bool IsKeyDown(int key);                                // Check if a key is being pressed
    // RLAPI bool IsKeyReleased(int key);                            // Check if a key has been released once
    // RLAPI bool IsKeyUp(int key);                                  // Check if a key is NOT being pressed
    // RLAPI int GetKeyPressed(void);                                // Get key pressed (keycode), call it multiple times for keys queued, returns 0 when the queue is empty
    // RLAPI int GetCharPressed(void);                               // Get char pressed (unicode), call it multiple times for chars queued, returns 0 when the queue is empty
    // RLAPI void SetExitKey(int key);

    static int native_key_down(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("key_down expects 1 argument (key code)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("key_down expects 1 argument (key code)");
            return 0;
        }

        int keyCode = (int)args[0].asNumber();
        bool isDown = IsKeyDown(keyCode) || isVirtualKeyDownByCode(keyCode);
        vm->pushBool(isDown);
        return 1;
    }

    static int native_key_pressed(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("key_pressed expects 1 argument (key code)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("key_pressed expects 1 argument (key code)");
            return 0;
        }

        int keyCode = (int)args[0].asNumber();
        bool isPressed = IsKeyPressed(keyCode) || isVirtualKeyPressedByCode(keyCode);
        vm->pushBool(isPressed);
        return 1;
    }

    static int native_key_released(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("key_released expects 1 argument (key code)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("key_released expects 1 argument (key code)");
            return 0;
        }

        int keyCode = (int)args[0].asNumber();
        bool isReleased = IsKeyReleased(keyCode) || isVirtualKeyReleasedByCode(keyCode);
        vm->pushBool(isReleased);
        return 1;
    }

    static int native_key_up(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("key_up expects 1 argument (key code)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("key_up expects 1 argument (key code)");
            return 0;
        }

        int keyCode = (int)args[0].asNumber();
        bool isUp = !IsKeyDown(keyCode) && !isVirtualKeyDownByCode(keyCode);
        vm->pushBool(isUp);
        return 1;
    }

    static int native_get_key_pressed(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_key_pressed expects no arguments");
            return 0;
        }

        int keyCode = GetKeyPressed();
        if (keyCode == 0)
        {
            for (size_t i = 0; i < s_virtualKeys.size(); i++)
            {
                if (s_virtualKeys[i].pressed)
                {
                    keyCode = s_virtualKeys[i].keyCode;
                    break;
                }
            }
        }
        vm->pushInt(keyCode);
        return 1;
    }

    static int native_get_char_pressed(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_char_pressed expects no arguments");
            return 0;
        }

        int charCode = GetCharPressed();
        vm->pushInt(charCode);
        return 1;
    }

    // Input-related functions: mouse
    // RLAPI bool IsMouseButtonPressed(int button);                  // Check if a mouse button has been pressed once
    // RLAPI bool IsMouseButtonDown(int button);                     // Check if a mouse button is being pressed
    // RLAPI bool IsMouseButtonReleased(int button);                 // Check if a mouse button has been released once
    // RLAPI bool IsMouseButtonUp(int button);                       // Check if a mouse button is NOT being pressed
    // RLAPI int GetMouseX(void);                                    // Get mouse position X
    // RLAPI int GetMouseY(void);                                    // Get mouse position Y
    // RLAPI Vector2 GetMousePosition(void);                         // Get mouse position XY
    // RLAPI Vector2 GetMouseDelta(void);                            // Get mouse delta between frames
    // RLAPI void SetMousePosition(int x, int y);                    // Set mouse position XY
    // RLAPI void SetMouseOffset(int offsetX, int offsetY);          // Set mouse offset
    // RLAPI void SetMouseScale(float scaleX, float scaleY);         // Set mouse scaling
    // RLAPI float GetMouseWheelMove(void);                          // Get mouse wheel movement for X or Y, whichever is larger
    // RLAPI Vector2 GetMouseWheelMoveV(void);                       // Get mouse wheel movement for both X and Y
    // RLAPI void SetMouseCursor(int cursor);                        // Set mouse cursor

    static int mousePressed(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("mouse_pressed expects 1 argument (button code)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("mouse_pressed expects 1 argument (button code)");
            return 0;
        }

        int buttonCode = (int)args[0].asNumber();
        bool isPressed = IsMouseButtonPressed(buttonCode);
        vm->pushBool(isPressed);
        return 1;
    }

    static int mouseDown(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("mouse_down expects 1 argument (button code)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("mouse_down expects 1 argument (button code)");
            return 0;
        }

        int buttonCode = (int)args[0].asNumber();
        bool isDown = IsMouseButtonDown(buttonCode);
        vm->pushBool(isDown);
        return 1;
    }

    static int mouseReleased(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("mouse_released expects 1 argument (button code)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("mouse_released expects 1 argument (button code)");
            return 0;
        }

        int buttonCode = (int)args[0].asNumber();
        bool isReleased = IsMouseButtonReleased(buttonCode);
        vm->pushBool(isReleased);
        return 1;
    }

    static int mouseUp(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("mouse_up expects 1 argument (button code)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("mouse_up expects 1 argument (button code)");
            return 0;
        }

        int buttonCode = (int)args[0].asNumber();
        bool isUp = IsMouseButtonUp(buttonCode);
        vm->pushBool(isUp);
        return 1;
    }

    static int getMouseX(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_mouse_x expects no arguments");
            return 0;
        }

        Vector2 worldPos = getMouseWorldPosition();
        vm->pushFloat(worldPos.x);
        return 1;
    }

    static int getMouseY(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_mouse_y expects no arguments");
            return 0;
        }

        Vector2 worldPos = getMouseWorldPosition();
        vm->pushFloat(worldPos.y);
        return 1;
    }

    static int getMousePosition(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_mouse_position expects no arguments");
            return 0;
        }

        Vector2 worldPos = getMouseWorldPosition();
        vm->pushFloat(worldPos.x);
        vm->pushFloat(worldPos.y);
       
        return 2;
    }

    static int getMouseScreenX(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_mouse_screen_x expects no arguments");
            return 0;
        }

        vm->pushInt(GetMouseX());
        return 1;
    }

    static int getMouseScreenY(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_mouse_screen_y expects no arguments");
            return 0;
        }

        vm->pushInt(GetMouseY());
        return 1;
    }

    static int getMouseScreenPosition(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_mouse_screen_position expects no arguments");
            return 0;
        }

        Vector2 pos = GetMousePosition();
        vm->pushFloat(pos.x);
        vm->pushFloat(pos.y);

        return 2;
    }

    static int getMouseDelta(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_mouse_delta expects no arguments");
            return 0;
        }

        Vector2 delta = GetMouseDelta();
        vm->pushFloat(delta.x);
        vm->pushFloat(delta.y);
       
        return 2;
    }

    static int getMouseWheel(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_mouse_wheel expects no arguments");
            return 0;
        }

        vm->pushFloat(GetMouseWheelMove());
        return 1;
    }

    static int getMouseWheelX(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_mouse_wheel_x expects no arguments");
            return 0;
        }

        Vector2 wheel = GetMouseWheelMoveV();
        vm->pushFloat(wheel.x);
        return 1;
    }

    static int getMouseWheelY(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_mouse_wheel_y expects no arguments");
            return 0;
        }

        Vector2 wheel = GetMouseWheelMoveV();
        vm->pushFloat(wheel.y);
        return 1;
    }

    static int setMousePosition(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_mouse_position expects 2 arguments (x, y)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_mouse_position expects 2 arguments (x, y)");
            return 0;
        }

        int x = (int)args[0].asNumber();
        int y = (int)args[1].asNumber();
        SetMousePosition(x, y);
        return 0;
    }

    static int setMouseOffset(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_mouse_offset expects 2 arguments (offsetX, offsetY)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_mouse_offset expects 2 arguments (offsetX, offsetY)");
            return 0;
        }

        int offsetX = (int)args[0].asNumber();
        int offsetY = (int)args[1].asNumber();
        SetMouseOffset(offsetX, offsetY);
        return 0;
    }

    static int setMouseScale(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_mouse_scale expects 2 arguments (scaleX, scaleY)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_mouse_scale expects 2 arguments (scaleX, scaleY)");
            return 0;
        }

        float scaleX = (float)args[0].asNumber();
        float scaleY = (float)args[1].asNumber();
        SetMouseScale(scaleX, scaleY);
        return 0;
    }

    static int hideCursor(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("hide_cursor expects no arguments");
            return 0;
        }
        HideCursor();
        return 0;
    }

    static int showCursor(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("show_cursor expects no arguments");
            return 0;
        }
        ShowCursor();
        return 0;
    }

    static int touchCount(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("touch_count expects no arguments");
            return 0;
        }
        vm->pushInt(GetTouchPointCount());
        return 1;
    }

    static int touchDown(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("touch_down expects 1 argument (index)");
            return 0;
        }

        int index = (int)args[0].asNumber();
        bool down = (index >= 0 && index < GetTouchPointCount());
        vm->pushBool(down);
        return 1;
    }

    static int touchPressedAny(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("touch_pressed_any expects no arguments");
            return 0;
        }
        vm->pushBool(s_anyTouchPressed);
        return 1;
    }

    static int touchReleasedAny(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("touch_released_any expects no arguments");
            return 0;
        }
        vm->pushBool(s_anyTouchReleased);
        return 1;
    }

    static int getTouchId(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("get_touch_id expects 1 argument (index)");
            return 0;
        }

        int index = (int)args[0].asNumber();
        if (index < 0 || index >= GetTouchPointCount())
        {
            vm->pushInt(-1);
            return 1;
        }

        vm->pushInt(GetTouchPointId(index));
        return 1;
    }

    static int getTouchScreenX(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("get_touch_screen_x expects 1 argument (index)");
            return 0;
        }

        int index = (int)args[0].asNumber();
        Vector2 pos = getTouchScreenPositionSafe(index);
        vm->pushFloat(pos.x);
        return 1;
    }

    static int getTouchScreenY(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("get_touch_screen_y expects 1 argument (index)");
            return 0;
        }

        int index = (int)args[0].asNumber();
        Vector2 pos = getTouchScreenPositionSafe(index);
        vm->pushFloat(pos.y);
        return 1;
    }

    static int getTouchScreenPosition(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("get_touch_screen_position expects 1 argument (index)");
            return 0;
        }

        int index = (int)args[0].asNumber();
        Vector2 pos = getTouchScreenPositionSafe(index);
        vm->pushFloat(pos.x);
        vm->pushFloat(pos.y);
        return 2;
    }

    static int getTouchX(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("get_touch_x expects 1 argument (index)");
            return 0;
        }

        int index = (int)args[0].asNumber();
        Vector2 pos = getTouchWorldPositionSafe(index);
        vm->pushFloat(pos.x);
        return 1;
    }

    static int getTouchY(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("get_touch_y expects 1 argument (index)");
            return 0;
        }

        int index = (int)args[0].asNumber();
        Vector2 pos = getTouchWorldPositionSafe(index);
        vm->pushFloat(pos.y);
        return 1;
    }

    static int getTouchPosition(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("get_touch_position expects 1 argument (index)");
            return 0;
        }

        int index = (int)args[0].asNumber();
        Vector2 pos = getTouchWorldPositionSafe(index);
        vm->pushFloat(pos.x);
        vm->pushFloat(pos.y);
        return 2;
    }

    static int getGesture(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_gesture expects no arguments");
            return 0;
        }
        vm->pushInt(GetGestureDetected());
        return 1;
    }

    static int gestureDetected(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("gesture_detected expects 1 argument (gesture flag)");
            return 0;
        }
        int gesture = (int)args[0].asNumber();
        vm->pushBool(IsGestureDetected(gesture));
        return 1;
    }

    static int virtualKeyAdd(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 5)
        {
            Error("vkey_add expects 5 arguments (key, x, y, w, h)");
            return 0;
        }
        for (int i = 0; i < 5; i++)
        {
            if (!args[i].isNumber())
            {
                Error("vkey_add expects numeric arguments (key, x, y, w, h)");
                return 0;
            }
        }

        VirtualKey vk = {};
        vk.keyCode = (int)args[0].asNumber();
        vk.bounds.x = (float)args[1].asNumber();
        vk.bounds.y = (float)args[2].asNumber();
        vk.bounds.width = (float)args[3].asNumber();
        vk.bounds.height = (float)args[4].asNumber();
        vk.down = false;
        vk.pressed = false;
        vk.released = false;

        if (vk.bounds.width < 0.0f)
        {
            vk.bounds.x += vk.bounds.width;
            vk.bounds.width = -vk.bounds.width;
        }
        if (vk.bounds.height < 0.0f)
        {
            vk.bounds.y += vk.bounds.height;
            vk.bounds.height = -vk.bounds.height;
        }

        s_virtualKeys.push_back(vk);
        return 0;
    }

    static int virtualKeyClear(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("vkey_clear expects no arguments");
            return 0;
        }
        s_virtualKeys.clear();
        return 0;
    }

    static int virtualKeyRemove(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("vkey_remove expects 1 argument (key)");
            return 0;
        }

        int keyCode = (int)args[0].asNumber();
        std::vector<VirtualKey> filtered;
        filtered.reserve(s_virtualKeys.size());
        for (size_t i = 0; i < s_virtualKeys.size(); i++)
        {
            if (s_virtualKeys[i].keyCode != keyCode)
                filtered.push_back(s_virtualKeys[i]);
        }
        s_virtualKeys.swap(filtered);
        return 0;
    }

    static int virtualKeyCount(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("vkey_count expects no arguments");
            return 0;
        }
        vm->pushInt((int)s_virtualKeys.size());
        return 1;
    }

    static int virtualKeySetVisible(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("vkey_set_visible expects 1 argument (bool)");
            return 0;
        }

        if (args[0].isBool())
            s_virtualKeysVisible = args[0].asBool();
        else if (args[0].isNumber())
            s_virtualKeysVisible = ((int)args[0].asNumber()) != 0;
        else
        {
            Error("vkey_set_visible expects bool/number");
            return 0;
        }

        return 0;
    }

    void registerAll(Interpreter &vm)
    {

        vm.registerNative("key_down", native_key_down, 1);
        vm.registerNative("key_pressed", native_key_pressed, 1);
        vm.registerNative("key_released", native_key_released, 1);
        vm.registerNative("key_up", native_key_up, 1);
        vm.registerNative("get_key_pressed", native_get_key_pressed, 0);
        vm.registerNative("get_char_pressed", native_get_char_pressed, 0);

        vm.registerNative("mouse_pressed", mousePressed, 1);
        vm.registerNative("mouse_down", mouseDown, 1);
        vm.registerNative("mouse_released", mouseReleased, 1);
        vm.registerNative("mouse_up", mouseUp, 1);
        
        vm.registerNative("get_mouse_x", getMouseX, 0);
        vm.registerNative("get_mouse_y", getMouseY, 0);
        vm.registerNative("get_mouse_position", getMousePosition, 0);
        vm.registerNative("get_mouse_screen_x", getMouseScreenX, 0);
        vm.registerNative("get_mouse_screen_y", getMouseScreenY, 0);
        vm.registerNative("get_mouse_screen_position", getMouseScreenPosition, 0);
        vm.registerNative("get_mouse_delta", getMouseDelta, 0);
        vm.registerNative("get_mouse_wheel", getMouseWheel, 0);
        vm.registerNative("get_mouse_wheel_x", getMouseWheelX, 0);
        vm.registerNative("get_mouse_wheel_y", getMouseWheelY, 0);
        vm.registerNative("set_mouse_position", setMousePosition, 2);
        vm.registerNative("set_mouse_offset", setMouseOffset, 2);
        vm.registerNative("set_mouse_scale", setMouseScale, 2);
        vm.registerNative("hide_cursor", hideCursor, 0);
        vm.registerNative("show_cursor", showCursor, 0);

        vm.registerNative("touch_count", touchCount, 0);
        vm.registerNative("touch_down", touchDown, 1);
        vm.registerNative("touch_pressed_any", touchPressedAny, 0);
        vm.registerNative("touch_released_any", touchReleasedAny, 0);
        vm.registerNative("get_touch_id", getTouchId, 1);
        vm.registerNative("get_touch_x", getTouchX, 1);
        vm.registerNative("get_touch_y", getTouchY, 1);
        vm.registerNative("get_touch_position", getTouchPosition, 1);
        vm.registerNative("get_touch_screen_x", getTouchScreenX, 1);
        vm.registerNative("get_touch_screen_y", getTouchScreenY, 1);
        vm.registerNative("get_touch_screen_position", getTouchScreenPosition, 1);
        vm.registerNative("get_gesture", getGesture, 0);
        vm.registerNative("gesture_detected", gestureDetected, 1);

        vm.registerNative("vkey_add", virtualKeyAdd, 5);
        vm.registerNative("vkey_clear", virtualKeyClear, 0);
        vm.registerNative("vkey_remove", virtualKeyRemove, 1);
        vm.registerNative("vkey_count", virtualKeyCount, 0);
        vm.registerNative("vkey_set_visible", virtualKeySetVisible, 1);
    }

} // namespace BindingsInput
