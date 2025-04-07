#pragma once


class TargetServo
{
private:
    int64_t m_tmLastActionTime;
public:
    int init(void);
    int setUp();
    int setDown();
    void stop();
    static TargetServo *getInstance();
};
