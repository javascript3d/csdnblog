#ifndef SYNCHRONIZATION_PROTOCOL_HPP
#define SYNCHRONIZATION_PROTOCOL_HPP

#include "network/protocol.hpp"
#include "utils/cpp2011.hpp"

#include <vector>
#include <map>

class SynchronizationProtocol : public Protocol
{
private:
    std::vector<std::map<uint32_t, double> > m_pings;
    std::vector<uint32_t> m_average_ping;

    /** Counts the number of pings sent. */
    uint32_t m_pings_count;
    std::vector<uint32_t> m_successed_pings;
    std::vector<double> m_total_diff;
    bool m_countdown_activated;
    double m_countdown;
    double m_last_countdown_update;
    bool m_has_quit;

    /** Keeps track of last time that an update was sent. */
    double m_last_time;


public:
             SynchronizationProtocol();
    virtual ~SynchronizationProtocol();

    virtual bool notifyEventAsynchronous(Event* event) OVERRIDE;
    virtual void setup() OVERRIDE;
    virtual void asynchronousUpdate() OVERRIDE;
    void startCountdown(int ms_countdown);

    // ------------------------------------------------------------------------
    virtual void update(float dt) OVERRIDE {}
    // ------------------------------------------------------------------------
    int getCountdown() { return (int)(m_countdown*1000.0); }

};   // class SynchronizationProtocol

#endif // SYNCHRONIZATION_PROTOCOL_HPP
