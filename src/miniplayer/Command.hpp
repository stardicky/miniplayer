#ifndef COMMAND_HPP
#define COMMAND_HPP

namespace miniplayer
{

typedef enum {
    Open = 2001,
    Stop
} CommandType;

struct Command
{
    CommandType type;
    uint64_t id;
    Command(CommandType type) :
        type(type)
    {
        static uint64_t IdSeed = 0;
        id = ++IdSeed;
    }
    virtual ~Command() {}
};

struct OpenCommand : public Command
{
    std::string mediaPath;
    OpenCommand(const std::string & mediaPath) :
        Command(CommandType::Open),
        mediaPath(mediaPath)
    {}
};

struct StopCommand : public Command
{
    StopCommand() :
        Command(CommandType::Stop)
    {}
};

}

#endif // COMMAND_HPP
