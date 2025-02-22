/****************************************************************************
**                                MIT License
**
** Copyright (C) 2020-2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Giuseppe D'Angelo <giuseppe.dangelo@kdab.com>
**
** This file is part of KDToolBox (https://github.com/KDAB/KDToolBox).
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, ** and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice (including the next paragraph)
** shall be included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF ** CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
****************************************************************************/

#include <KDSignalThrottler.h>

#include <QSignalSpy>
#include <QTest>

#include <memory>

using namespace KDToolBox;

class tst_KDSignalThrottler : public QObject
{
    Q_OBJECT

    static constexpr int Timeout = 50;

private:
    template <typename Throttler>
    void basics_impl(KDGenericSignalThrottler::Kind kind,
                     KDGenericSignalThrottler::EmissionPolicy emissionPolicy);
    template <typename Throttler>
    void trailingOrLeadingActivation_impl();

private Q_SLOTS:
    void basics();
    void trailingOrLeadingActivation();
    void throttler();
    void debouncer();
};

void tst_KDSignalThrottler::basics()
{
    basics_impl<KDSignalThrottler>(KDGenericSignalThrottler::Kind::Throttler,
                                   KDGenericSignalThrottler::EmissionPolicy::Trailing);
    basics_impl<KDSignalLeadingThrottler>(KDGenericSignalThrottler::Kind::Throttler,
                                   KDGenericSignalThrottler::EmissionPolicy::Leading);
    basics_impl<KDSignalDebouncer>(KDGenericSignalThrottler::Kind::Debouncer,
                                   KDGenericSignalThrottler::EmissionPolicy::Trailing);
    basics_impl<KDSignalLeadingDebouncer>(KDGenericSignalThrottler::Kind::Debouncer,
                                          KDGenericSignalThrottler::EmissionPolicy::Leading);
}

template<typename Throttler>
void tst_KDSignalThrottler::basics_impl(KDGenericSignalThrottler::Kind kind,
                                        KDGenericSignalThrottler::EmissionPolicy emissionPolicy)
{
    Throttler t;
    QCOMPARE(t.kind(), kind);
    QCOMPARE(t.emissionPolicy(), emissionPolicy);
    QCOMPARE(t.timeout(), 0);
    QCOMPARE(t.timerType(), Qt::CoarseTimer);

    t.setTimeout(100);
    QCOMPARE(t.timeout(), 100);

    using namespace std::chrono_literals;
    t.setTimeout(123ms);
    QCOMPARE(t.timeout(), 123);

    t.setTimeout(2s);
    QCOMPARE(t.timeout(), 2000);

    t.setTimeout(0);
    QCOMPARE(t.timeout(), 0);

    t.setTimerType(Qt::PreciseTimer);
    QCOMPARE(t.timerType(), Qt::PreciseTimer);

    t.setTimerType(Qt::CoarseTimer);
    QCOMPARE(t.timerType(), Qt::CoarseTimer);
}

void tst_KDSignalThrottler::trailingOrLeadingActivation()
{
    trailingOrLeadingActivation_impl<KDSignalThrottler>();
    trailingOrLeadingActivation_impl<KDSignalLeadingThrottler>();
    trailingOrLeadingActivation_impl<KDSignalDebouncer>();
    trailingOrLeadingActivation_impl<KDSignalLeadingDebouncer>();
}

template<typename Throttler>
void tst_KDSignalThrottler::trailingOrLeadingActivation_impl()
{
    // Just basics, nothing gets emitted
    {
        auto t = std::make_unique<Throttler>();

        QSignalSpy spy(t.get(), &Throttler::triggered);
        QVERIFY(spy.isValid());
        QCOMPARE(spy.size(), 0);

        t.reset();
        QCOMPARE(spy.size(), 0);
    }

    // Activate only once
    {
        auto t = std::make_unique<Throttler>();
        const auto ep = t->emissionPolicy();

        QSignalSpy spy(t.get(), &Throttler::triggered);
        int triggeredCount = 0;
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();

        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: ++triggeredCount; break;
        }
        QCOMPARE(spy.size(), triggeredCount);

        QCoreApplication::processEvents(QEventLoop::AllEvents, 0);

        triggeredCount = 1;

        QCOMPARE(spy.size(), triggeredCount);
        t.reset();
        QCOMPARE(spy.size(), triggeredCount);
    }

    // Activate more than once. Get the leading signal, then another one
    // when the throttler/debouncer also activates. Done with a 0 timeout,
    // so processEvents is sufficient to activate
    {
        auto t = std::make_unique<Throttler>();
        const auto ep = t->emissionPolicy();

        QSignalSpy spy(t.get(), &Throttler::triggered);
        int triggeredCount = 0;
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();

        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: ++triggeredCount; break;
        }
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();
        t->throttle();
        QCOMPARE(spy.size(), triggeredCount);

        QCoreApplication::processEvents(QEventLoop::AllEvents, 0);

        ++triggeredCount;

        QCOMPARE(spy.size(), triggeredCount);
        t.reset();
        QCOMPARE(spy.size(), triggeredCount);
    }

    // Same, but with non-zero timeout
    {
        auto t = std::make_unique<Throttler>();
        const auto ep = t->emissionPolicy();
        t->setTimeout(Timeout);

        QSignalSpy spy(t.get(), &Throttler::triggered);
        int triggeredCount = 0;
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();

        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: ++triggeredCount; break;
        }
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();
        t->throttle();
        QCOMPARE(spy.size(), triggeredCount);

        ++triggeredCount;

        QTRY_COMPARE(spy.size(), triggeredCount);

        t.reset();
        QCOMPARE(spy.size(), triggeredCount);
    }

    // Don't wait for a timeout, instead check that it does trigger on deletion
    {
        auto t = std::make_unique<Throttler>();
        const auto ep = t->emissionPolicy();
        t->setTimeout(Timeout);

        QSignalSpy spy(t.get(), &Throttler::triggered);
        int triggeredCount = 0;
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();

        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: ++triggeredCount; break;
        }
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();
        t->throttle();
        QCOMPARE(spy.size(), triggeredCount);

        t.reset();
        ++triggeredCount;
        QCOMPARE(spy.size(), triggeredCount);
    }

    // Trailing: no activation immediately, no activity until timeout => emission
    // Leading: activation, then no activity until timeout => no further emission
    {
        auto t = std::make_unique<Throttler>();
        const auto ep = t->emissionPolicy();
        t->setTimeout(Timeout);

        QSignalSpy spy(t.get(), &Throttler::triggered);
        int triggeredCount = 0;
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();

        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: ++triggeredCount; break;
        }
        QCOMPARE(spy.size(), triggeredCount);

        QTest::qWait(Timeout * 2);

        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: ++triggeredCount; break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: break;
        }
        QCOMPARE(spy.size(), triggeredCount);
    }

    // Trailing: no activation immediately, activity after timeout, no new immediate activation
    // Leading: activation, no activity until timeout, new leading activation
    {
        auto t = std::make_unique<Throttler>();
        const auto ep = t->emissionPolicy();
        t->setTimeout(Timeout);

        QSignalSpy spy(t.get(), &Throttler::triggered);
        int triggeredCount = 0;
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();

        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: ++triggeredCount; break;
        }
        QCOMPARE(spy.size(), triggeredCount);

        QTest::qWait(Timeout * 2);
        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: ++triggeredCount; break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: break;
        }
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();
        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: ++triggeredCount; break;
        }
        QCOMPARE(spy.size(), triggeredCount);

        t.reset();
        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: ++triggeredCount; break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: break;
        }
        QCOMPARE(spy.size(), triggeredCount);
    }


    // Same, but throttle more signals
    {
        auto t = std::make_unique<Throttler>();
        const auto ep = t->emissionPolicy();
        t->setTimeout(Timeout);

        QSignalSpy spy(t.get(), &Throttler::triggered);
        int triggeredCount = 0;
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();

        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: ++triggeredCount; break;
        }
        QCOMPARE(spy.size(), triggeredCount);

        QTest::qWait(Timeout * 2);
        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: ++triggeredCount; break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: break;
        }
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();
        switch (ep) {
        case KDGenericSignalThrottler::EmissionPolicy::Trailing: break;
        case KDGenericSignalThrottler::EmissionPolicy::Leading: ++triggeredCount; break;
        }
        QCOMPARE(spy.size(), triggeredCount);

        t->throttle();
        t->throttle();

        t.reset();
        ++triggeredCount;
        QCOMPARE(spy.size(), triggeredCount);
    }
}

void tst_KDSignalThrottler::throttler()
{
    KDSignalThrottler throttler;
    throttler.setTimeout(Timeout);
    throttler.setTimerType(Qt::PreciseTimer);

    QSignalSpy spy(&throttler, &KDGenericSignalThrottler::triggered);
    throttler.throttle();
    QCOMPARE(spy.size(), 0);

    constexpr int Count = 20;
    constexpr int Frequency = 5;

    for (int i = 0; i < Count; ++i) {
        // Basic throttler behavior: signal is emitted "less often"
        // than the incoming frequency (ideally, at its own timeout)
        QTest::qWait(Timeout / Frequency);
        throttler.throttle();
    }

    // This is a bit sketchy due to the possible timer inaccuracies
    // and roundings, but should be "sound".
    constexpr int ThrottledCount = Count / Frequency;
    QTRY_VERIFY(spy.size() == ThrottledCount || spy.size() == ThrottledCount + 1);
}

void tst_KDSignalThrottler::debouncer()
{
    KDSignalDebouncer debouncer;
    debouncer.setTimeout(Timeout);

    QSignalSpy spy(&debouncer, &KDGenericSignalThrottler::triggered);
    debouncer.throttle();
    QCOMPARE(spy.size(), 0);

    constexpr int Count = 20;
    for (int i = 0; i < Count; ++i) {
        // Basic debouncer behavior: incoming notifications before the
        // timeout will reset the timeout
        QTest::qWait(Timeout / 5);
        debouncer.throttle();
        QCOMPARE(spy.size(), 0);
    }

    // Eventually, this will trigger only 1 activation
    QTRY_COMPARE(spy.size(), 1);
}

QTEST_MAIN(tst_KDSignalThrottler)

#include "tst_KDSignalThrottler.moc"
