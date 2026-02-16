#include "bindings.hpp"
#include "interpreter.hpp"
#include <unordered_map>
#include <deque>

namespace BindingsMessage
{

    struct Message
    {
        uint32 from;
        Value type;
        Value data;
    };

    static std::unordered_map<uint32, std::deque<Message>> messages;


    int native_send(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 3)
        {
            Error("send expects 3 arguments: target (process), message type, message value");
            vm->pushBool(false);
            return 1;
        }
        bool delivered = false;

        // send(process_instance, type, value)
        if (args[0].isProcessInstance())
        {
            Process *target = args[0].asProcess();
            uint32 toID = target->id;
            messages[toID].push_back({proc->id, args[1], args[2]});
            delivered = true;

          //  Info("Message sent from process %u to process %u", proc->id, toID);
        }
        else if (args[0].isInt())
        {
            // send(type, type, value) - broadcast to all processes of this blueprint
            int target = args[0].asInt();
            
             const auto &alive = vm->getAliveProcesses();
            for (size_t i = 0; i < alive.size(); i++)
            {
                Process *toProc = alive[i];
                if (toProc && toProc->blueprint == target && toProc->state != ProcessState::DEAD)
                {
                    messages[toProc->id].push_back({proc->id, args[1], args[2]});
                    delivered = true;

                  //  Info("Message sent from process %u to process ID %u", proc->id, target);
                }
            }

            
        }

        vm->pushBool(delivered);
        return 1;
    }

    int native_clean_messages(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("clean_messages expects 0 arguments");
            return 0;
        }

        messages.erase(proc->id);
        return 0;
    }

    int native_clear_messages(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("clear_messages expects 0 arguments");
            return 0;
        }

        messages.clear();
        return 0;
    }
 

    // has_messages_from(type) - check if this process has messages of specific type
    int native_has_messages_from(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("has_messages_from expects 1 argument (type)");
            vm->pushBool(false);
            return 1;
        }
        auto it = messages.find(proc->id);
        if (it != messages.end())
        {
            for (const Message &msg : it->second)
            {
                if (msg.type.type == args[0].type)
                {
                    vm->pushBool(true);
                    return 1;
                }
            }
        }
        vm->pushBool(false);
        return 1;
    }

   int native_pop_message(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("pop_message expects 0 arguments");
            vm->pushNil();
            return 1;
        }
        
        auto it = messages.find(proc->id);
        if (it != messages.end() && !it->second.empty())
        {
            Message msg = it->second.front();
            it->second.pop_front();
            vm->push(msg.data);
        }
        else
        {
            vm->pushNil();
        }

        return 1;
    }

       int native_pop_ex_message(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("pop_ex_message expects 0 arguments");
            vm->pushNil();
            vm->pushNil();
            return 2;
        }
        
        auto it = messages.find(proc->id);
        if (it != messages.end() && !it->second.empty())
        {
            Message msg = it->second.front();
            it->second.pop_front();
            vm->push(msg.data);
            vm->push(vm->makeProcessInstance(vm->findProcessById(msg.from)));
        }
        else
        {
            vm->pushNil();
            vm->pushNil();
        }

        return 2;
    }

    int native_count_messages(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("count_messages expects 0 arguments");
            vm->pushInt(0);
            return 1;
        }
        
        auto it = messages.find(proc->id);
        if (it != messages.end())
        {
            vm->pushInt((int)it->second.size());
        }
        else
        {
            vm->pushInt(0);
        }

        return 1;
    }

    int native_peek_message(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("peek_message expects 1 argument (index)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isInt())
        {
            Error("peek_message expects 1 integer argument (index)");
            vm->pushNil();
            return 1;
        }
        int index = args[0].asInt();
        auto it = messages.find(proc->id);
        if (it != messages.end() && index >= 0 && index < (int)it->second.size())
        {            
            Message msg = it->second[index];
            Value messageValue = msg.data;
            vm->push(messageValue);
        }
        else        
        {
            vm->pushNil();
        }

        return 1;
    }


    void clearAllMessages()
    {
        messages.clear();
    }

    void registerAll(Interpreter &vm)
    {
        vm.registerNativeProcess("send", native_send, 3);
        vm.registerNativeProcess("clean_messages", native_clean_messages, 0);
        vm.registerNativeProcess("clear_messages", native_clear_messages, 0);
        vm.registerNativeProcess("has_message", native_has_messages_from, 1);
        vm.registerNativeProcess("pop_message", native_pop_message, 0);
        vm.registerNativeProcess("pop_ex_message", native_pop_ex_message, 0);
        vm.registerNativeProcess("count_messages", native_count_messages, 0);
        vm.registerNativeProcess("peek_message", native_peek_message, 1);
    }
}
