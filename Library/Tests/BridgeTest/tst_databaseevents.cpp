#include "testHelpers.h"

#include <QTest>
#include <QTimeZone>

using namespace dsqt::bridge;
using namespace testing;

namespace {
const QDate kMonday(2026, 9, 28);   // First day of the multi-day ranges below.
const QDate kNextMonday(2026, 10, 5);
const QDate kSunday(2026, 10, 4);
const QDate kTomorrow(2026, 10, 2);

constexpr int kWednesday = 0x08;
constexpr int kSundayBit = 0x01;
constexpr int kSaturday  = 0x40;

DatabaseRecordList list(std::initializer_list<DatabaseRecord> records) {
    return DatabaseRecordList(records);
}

/// A date and time in UTC. Used as a plain container for a wall-clock time, because UTC has no DST gaps or repeated
/// hours, and also to describe exact instants.
QDateTime utc(const QDate& date, int hour, int minute = 0, int second = 0) {
    const QTime time(hour, minute, second);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QDateTime(date, time, QTimeZone::UTC);
#else
    return QDateTime(date, time, Qt::UTC);
#endif
}

/// A fixed-offset time zone, such as UTC+02:00.
QTimeZone fixedOffset(int seconds) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QTimeZone::fromSecondsAheadOfUtc(seconds);
#else
    return QTimeZone(seconds);
#endif
}
} // namespace

// Tests for the event helpers in dsBridgeDatabase.h: predicates, filters, sorting, timelines and
// DatabaseContent::getCurrentEvent().
//
// Weekdays are a bitmask: bit 0 is Sunday, bit 1 Monday, ... bit 6 Saturday. Thursday 1 October 2026 is bit 4.
class DatabaseEventsTest : public QObject {
    Q_OBJECT

  private slots:
    void isToday_data();
    void isToday();

    void isNow_data();
    void isNow();
    void isNow_usesTimeOfDayOnEveryDayOfTheRange();

    void isWithinSpan_data();
    void isWithinSpan();
    void isWithinSpan_invalidEvent();

    void secondsSinceMidnight();
    void durationInSeconds();

    void filterEvents_byType();
    void filterEvents_byPlatform();
    void filterEvents_byDateTime();
    void filterEvents_byDate();
    void filterEvents_bySpan();

    void sortEvents_activeBeforeInactive_laterStartFirst();
    void sortEvents_shorterDurationWins();
    void sortEvents_fewerDaysWins();
    void sortEvents_uidBreaksTies();

    void eventTimeline_emptyInput();
    void eventTimeline_notScheduledToday();
    void eventTimeline_singleEvent_data();
    void eventTimeline_singleEvent();
    void eventTimeline_overlappingEvent_splitsTheLongerOne();

    void getCurrentEvent();

    void isNow_overnight_data();
    void isNow_overnight();
    void durationInSeconds_overnight();
    void eventTimeline_overnightEvent();
    void getCurrentEvent_overnightEvent();

    void isNow_readsTheWallClockOfTheGivenDateTime();
    void isWithinSpan_comparesInstants();
    void isNow_dstFallBack_bothOccurrencesOfTheRepeatedHour();
    void isNow_dstSpringForward_continuesAcrossTheGap();
    void durationInSeconds_isWallClockTimeOnDstDays();
};

// ---- Predicates ------------------------------------------------------------------------------------------------

void DatabaseEventsTest::isToday_data() {
    QTest::addColumn<QDate>("date");
    QTest::addColumn<QDateTime>("start");
    QTest::addColumn<QDateTime>("end");
    QTest::addColumn<int>("days");
    QTest::addColumn<bool>("expected");

    const QDateTime start(kMonday, QTime(9, 0));
    const QDateTime end(kNextMonday, QTime(17, 0));

    QTest::newRow("inside the range") << kToday << start << end << kAllDays << true;
    QTest::newRow("first day") << kMonday << start << end << kAllDays << true;
    QTest::newRow("last day") << kNextMonday << start << end << kAllDays << true;
    QTest::newRow("day before") << kMonday.addDays(-1) << start << end << kAllDays << false;
    QTest::newRow("day after") << kNextMonday.addDays(1) << start << end << kAllDays << false;
    QTest::newRow("weekday is set") << kToday << start << end << kThursday << true;
    QTest::newRow("weekday is not set") << kToday << start << end << kWednesday << false;
    QTest::newRow("sunday is bit 0") << kSunday << start << end << kSundayBit << true;
    QTest::newRow("sunday is not saturday") << kSunday << start << end << kSaturday << false;
    QTest::newRow("no days at all") << kToday << start << end << 0 << false;
    QTest::newRow("invalid start") << kToday << QDateTime() << end << kAllDays << false;
    QTest::newRow("invalid end") << kToday << start << QDateTime() << kAllDays << false;
}

void DatabaseEventsTest::isToday() {
    QFETCH(QDate, date);
    QFETCH(QDateTime, start);
    QFETCH(QDateTime, end);
    QFETCH(int, days);
    QFETCH(bool, expected);

    QCOMPARE(dsqt::bridge::isToday(date, start, end, days), expected);
}

void DatabaseEventsTest::isNow_data() {
    QTest::addColumn<QDateTime>("now");
    QTest::addColumn<int>("days");
    QTest::addColumn<bool>("expected");

    // The event runs from 09:00 until 17:00 on kToday.
    QTest::newRow("a second before the start") << at(8, 59, 59) << kAllDays << false;
    QTest::newRow("at the start") << at(9, 0) << kAllDays << true;
    QTest::newRow("in the middle") << at(12, 0) << kAllDays << true;
    QTest::newRow("a second before the end") << at(16, 59, 59) << kAllDays << true;
    QTest::newRow("at the end") << at(17, 0) << kAllDays << false;
    QTest::newRow("after the end") << at(18, 0) << kAllDays << false;
    QTest::newRow("other date") << at(12, 0, 0, kTomorrow) << kAllDays << false;
    QTest::newRow("weekday is set") << at(12, 0) << kThursday << true;
    QTest::newRow("weekday is not set") << at(12, 0) << kWednesday << false;
}

void DatabaseEventsTest::isNow() {
    QFETCH(QDateTime, now);
    QFETCH(int, days);
    QFETCH(bool, expected);

    QCOMPARE(dsqt::bridge::isNow(now, at(9, 0), at(17, 0), days), expected);
}

void DatabaseEventsTest::isNow_usesTimeOfDayOnEveryDayOfTheRange() {
    const QDateTime start(kMonday, QTime(9, 0));
    const QDateTime end(kNextMonday, QTime(17, 0));
    const QDate     day(2026, 10, 3);

    QVERIFY(dsqt::bridge::isNow(at(10, 0, 0, day), start, end));
    QVERIFY(!dsqt::bridge::isNow(at(8, 0, 0, day), start, end));
    QVERIFY(!dsqt::bridge::isNow(at(18, 0, 0, day), start, end));
}

void DatabaseEventsTest::isWithinSpan_data() {
    QTest::addColumn<QDateTime>("spanStart");
    QTest::addColumn<QDateTime>("spanEnd");
    QTest::addColumn<bool>("expected");

    // The event runs from 10:00 until 12:00 on kToday.
    QTest::newRow("overlaps the start") << at(9) << at(11) << true;
    QTest::newRow("overlaps the end") << at(11) << at(13) << true;
    QTest::newRow("inside the event") << at(10, 30) << at(11, 30) << true;
    QTest::newRow("contains the event") << at(8) << at(14) << true;
    QTest::newRow("touches the start") << at(8) << at(10) << true;
    QTest::newRow("touches the end") << at(12) << at(14) << true;
    QTest::newRow("before the event") << at(7) << at(9) << false;
    QTest::newRow("after the event") << at(13) << at(14) << false;
    QTest::newRow("span ends before it starts") << at(11) << at(10) << false;
    QTest::newRow("invalid span") << QDateTime() << QDateTime() << false;
}

void DatabaseEventsTest::isWithinSpan() {
    QFETCH(QDateTime, spanStart);
    QFETCH(QDateTime, spanEnd);
    QFETCH(bool, expected);

    QCOMPARE(dsqt::bridge::isWithinSpan(spanStart, spanEnd, at(10), at(12)), expected);
}

void DatabaseEventsTest::isWithinSpan_invalidEvent() {
    QVERIFY(!dsqt::bridge::isWithinSpan(at(9), at(13), QDateTime(), at(12)));
    QVERIFY(!dsqt::bridge::isWithinSpan(at(9), at(13), at(10), QDateTime()));
    QVERIFY(!dsqt::bridge::isWithinSpan(at(9), at(13), at(12), at(10))); // Ends before it starts.
}

void DatabaseEventsTest::secondsSinceMidnight() {
    QCOMPARE(dsqt::bridge::secondsSinceMidnight(at(0, 0)), 0);
    QCOMPARE(dsqt::bridge::secondsSinceMidnight(at(1, 0, 5)), 3605);
    QCOMPARE(dsqt::bridge::secondsSinceMidnight(at(23, 59, 59)), 86399);
}

void DatabaseEventsTest::durationInSeconds() {
    QCOMPARE(dsqt::bridge::durationInSeconds(at(9), at(17)), 8 * 3600);
    QCOMPARE(dsqt::bridge::durationInSeconds(at(9), at(9)), 0);
    QCOMPARE(dsqt::bridge::durationInSeconds(at(10), at(9)), -3600);
    // Only the time of day counts, not the date.
    QCOMPARE(dsqt::bridge::durationInSeconds(at(9), at(17, 0, 0, kNextMonday)), 8 * 3600);
}

// ---- Filters ---------------------------------------------------------------------------------------------------

void DatabaseEventsTest::filterEvents_byType() {
    DatabaseRecordList events =
        list({makeEvent("a", QTime(9, 0), QTime(10, 0), kAllDays, {}, "A"),
              makeEvent("b", QTime(9, 0), QTime(10, 0), kAllDays, {}, "B"),
              makeEvent("c", QTime(9, 0), QTime(10, 0), kAllDays, {}, "A"),
              makeEvent("d", QTime(9, 0), QTime(10, 0), kAllDays, {}, "")});

    // An empty type name means "do not filter".
    QCOMPARE(filterEvents(events, QString()), size_t(0));
    QCOMPARE(events.size(), qsizetype(4));

    QCOMPARE(filterEvents(events, QString("A")), size_t(2));
    QCOMPARE(uids(events), (QStringList{"a", "c"}));
}

void DatabaseEventsTest::filterEvents_byPlatform() {
    const DatabaseRecord platform = makeRecord("P1", "ROOT_PLATFORM", 1);
    DatabaseRecordList   events   = list({makeEvent("e1", QTime(9, 0), QTime(10, 0), kAllDays, {"P1"}),
                                          makeEvent("e2", QTime(9, 0), QTime(10, 0), kAllDays, {"P2"}),
                                          makeEvent("e3", QTime(9, 0), QTime(10, 0), kAllDays, {"P1", "P2"}),
                                          makeEvent("e4", QTime(9, 0), QTime(10, 0), kAllDays, {})});

    // Only platform records can be used as a filter.
    QCOMPARE(filterEvents(events, makeRecord("X", "", 1)), size_t(0));
    QCOMPARE(filterEvents(events, DatabaseRecord()), size_t(0));
    QCOMPARE(events.size(), qsizetype(4));

    QCOMPARE(filterEvents(events, platform), size_t(2));
    QCOMPARE(uids(events), (QStringList{"e1", "e3"}));
}

void DatabaseEventsTest::filterEvents_byDateTime() {
    DatabaseRecordList events =
        list({makeEvent("active", QTime(9, 0), QTime(17, 0)),
              makeEvent("ended", QTime(8, 0), QTime(9, 0)),
              makeEvent("later", QTime(13, 0), QTime(14, 0)),
              makeEvent("wrong weekday", QTime(9, 0), QTime(17, 0), kWednesday),
              makeEvent("other date", QTime(9, 0), QTime(17, 0), kAllDays, {}, {}, kTomorrow, kTomorrow)});

    QCOMPARE(filterEvents(events, at(12)), size_t(4));
    QCOMPARE(uids(events), (QStringList{"active"}));
}

void DatabaseEventsTest::filterEvents_byDate() {
    DatabaseRecordList events =
        list({makeEvent("today", QTime(9, 0), QTime(17, 0)),
              makeEvent("tomorrow", QTime(9, 0), QTime(17, 0), kAllDays, {}, {}, kTomorrow, kTomorrow),
              makeEvent("wrong weekday", QTime(9, 0), QTime(17, 0), kWednesday),
              makeEvent("long range", QTime(9, 0), QTime(17, 0), kAllDays, {}, {}, kMonday, kNextMonday)});

    // The time of day does not matter, only the date and the weekday.
    QCOMPARE(filterEvents(events, kToday), size_t(2));
    QCOMPARE(uids(events), (QStringList{"today", "long range"}));
}

void DatabaseEventsTest::filterEvents_bySpan() {
    DatabaseRecordList events =
        list({makeEvent("overlapping", QTime(11, 30), QTime(12, 30)),
              makeEvent("before", QTime(8, 0), QTime(9, 0)),
              makeEvent("after", QTime(13, 0), QTime(14, 0)),
              makeEvent("touching", QTime(12, 0), QTime(13, 0)),
              makeEvent("other weekday", QTime(11, 0), QTime(12, 0), kWednesday)});

    // Weekdays are not checked.
    QCOMPARE(filterEvents(events, at(11), at(12)), size_t(2));
    QCOMPARE(uids(events), (QStringList{"overlapping", "touching", "other weekday"}));
}

// ---- Sorting ---------------------------------------------------------------------------------------------------

void DatabaseEventsTest::sortEvents_activeBeforeInactive_laterStartFirst() {
    // At 12:00: B and A are active (B started more recently), D started earlier today and has ended,
    // C has not started yet. Events that have started rank above events that still have to start.
    DatabaseRecordList events = list({makeEvent("A", QTime(9, 0), QTime(17, 0)),
                                      makeEvent("C", QTime(13, 0), QTime(14, 0)),
                                      makeEvent("D", QTime(8, 0), QTime(9, 0)),
                                      makeEvent("B", QTime(11, 0), QTime(13, 0))});
    sortEvents(events, at(12));
    QCOMPARE(uids(events), (QStringList{"B", "A", "D", "C"}));
}

void DatabaseEventsTest::sortEvents_shorterDurationWins() {
    DatabaseRecordList events =
        list({makeEvent("long", QTime(10, 0), QTime(14, 0)), makeEvent("short", QTime(10, 0), QTime(12, 30))});
    sortEvents(events, at(12));
    QCOMPARE(uids(events), (QStringList{"short", "long"}));
}

void DatabaseEventsTest::sortEvents_fewerDaysWins() {
    DatabaseRecordList events = list({makeEvent("every day", QTime(10, 0), QTime(14, 0), kAllDays),
                                      makeEvent("thursdays", QTime(10, 0), QTime(14, 0), kThursday)});
    sortEvents(events, at(12));
    QCOMPARE(uids(events), (QStringList{"thursdays", "every day"}));
}

void DatabaseEventsTest::sortEvents_uidBreaksTies() {
    DatabaseRecordList events =
        list({makeEvent("b", QTime(10, 0), QTime(14, 0)), makeEvent("a", QTime(10, 0), QTime(14, 0))});
    sortEvents(events, at(12));
    QCOMPARE(uids(events), (QStringList{"a", "b"}));
}

// ---- Timeline --------------------------------------------------------------------------------------------------

void DatabaseEventsTest::eventTimeline_emptyInput() {
    QVERIFY(eventTimeline({}, kToday).isEmpty());
}

void DatabaseEventsTest::eventTimeline_notScheduledToday() {
    const auto events = list({makeEvent("other day", QTime(9, 0), QTime(17, 0), kAllDays, {}, {}, kTomorrow, kTomorrow),
                              makeEvent("wrong weekday", QTime(9, 0), QTime(17, 0), kWednesday)});
    QVERIFY(eventTimeline(events, kToday).isEmpty());
}

void DatabaseEventsTest::eventTimeline_singleEvent_data() {
    QTest::addColumn<QDate>("firstDate");
    QTest::addColumn<QDate>("lastDate");

    QTest::newRow("scheduled on one day") << kToday << kToday;
    QTest::newRow("scheduled over a range of days") << kMonday << kNextMonday;
}

void DatabaseEventsTest::eventTimeline_singleEvent() {
    QFETCH(QDate, firstDate);
    QFETCH(QDate, lastDate);

    const auto events   = list({makeEvent("solo", QTime(9, 0), QTime(17, 0), kAllDays, {}, {}, firstDate, lastDate)});
    const auto timeline = eventTimeline(events, kToday);

    QCOMPARE(timeline.size(), qsizetype(1));
    QCOMPARE(timeline[0].uid(), QString("solo"));
    QCOMPARE(timeline[0].start().time(), QTime(9, 0));
    QCOMPARE(timeline[0].end().time(), QTime(17, 0));
}

// X runs from 09:00 until 12:00, Y from 10:00 until 11:00. Y has the higher priority while it is active (it started
// more recently), so X is interrupted: X, Y, X.
void DatabaseEventsTest::eventTimeline_overlappingEvent_splitsTheLongerOne() {
    const auto events   = list({makeEvent("X", QTime(9, 0), QTime(12, 0)), makeEvent("Y", QTime(10, 0), QTime(11, 0))});
    const auto timeline = eventTimeline(events, kToday);

    QCOMPARE(uids(timeline), (QStringList{"X", "Y", "X"}));
    QCOMPARE(timeline[0].start().time(), QTime(9, 0));
    QCOMPARE(timeline[0].end().time(), QTime(10, 0));
    QCOMPARE(timeline[1].start().time(), QTime(10, 0));
    QCOMPARE(timeline[1].end().time(), QTime(11, 0));
    QCOMPARE(timeline[2].start().time(), QTime(11, 0));
    QCOMPARE(timeline[2].end().time(), QTime(12, 0));
}

// ---- DatabaseContent -------------------------------------------------------------------------------------------

void DatabaseEventsTest::getCurrentEvent() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content;
    Access::add(content, makeEvent("A", QTime(9, 0), QTime(17, 0)));
    Access::add(content, makeEvent("B", QTime(11, 0), QTime(13, 0)));
    Access::add(content, makeEvent("C", QTime(13, 0), QTime(14, 0)));

    QVERIFY(content.getCurrentEvent(at(8)).isEmpty());                    // Nothing has started.
    QCOMPARE(content.getCurrentEvent(at(10)).uid(), QString("A"));        // Only A is active.
    QCOMPARE(content.getCurrentEvent(at(12)).uid(), QString("B"));        // B started more recently than A.
    QCOMPARE(content.getCurrentEvent(at(13, 30)).uid(), QString("C"));    // B has ended, C started after A.
    QCOMPARE(content.getCurrentEvent(at(14, 30)).uid(), QString("A"));    // Back to A.
    QVERIFY(content.getCurrentEvent(at(18)).isEmpty());                   // Everything has ended.
}

// ---- Overnight events ------------------------------------------------------------------------------------------
//
// An event stores a range of dates and a daily window: a start time and an end time. An overnight event, such as
// 22:00 until 02:00, has an end time that is earlier than its start time. isNow() checks "start <= time < end", which
// can never be true for such a window, durationInSeconds() is negative, and eventTimeline() throws the event away
// as an invalid duration.
//
// Whether the CMS allows overnight events is not known. These tests describe what the helpers should do if it does,
// and are marked as known failures until that is decided. If overnight events turn out to be impossible, delete the
// rows and tests instead of fixing the helpers.

void DatabaseEventsTest::isNow_overnight_data() {
    QTest::addColumn<QTime>("start");
    QTest::addColumn<QTime>("end");
    QTest::addColumn<QTime>("now");
    QTest::addColumn<bool>("expected");
    QTest::addColumn<bool>("knownBug");

    const QTime ten(22, 0);
    QTest::newRow("22:00-02:00, at the start") << ten << QTime(2, 0) << QTime(22, 0) << true << true;
    QTest::newRow("22:00-02:00, late evening") << ten << QTime(2, 0) << QTime(23, 30) << true << true;
    QTest::newRow("22:00-02:00, a second before the start") << ten << QTime(2, 0) << QTime(21, 59, 59) << false << false;
    QTest::newRow("22:00-02:00, midday") << ten << QTime(2, 0) << QTime(12, 0) << false << false;
    QTest::newRow("22:00-00:00, late evening") << ten << QTime(0, 0) << QTime(23, 30) << true << true;
    QTest::newRow("22:00-00:00, a second before the start") << ten << QTime(0, 0) << QTime(21, 59, 59) << false << false;
}

void DatabaseEventsTest::isNow_overnight() {
    QFETCH(QTime, start);
    QFETCH(QTime, end);
    QFETCH(QTime, now);
    QFETCH(bool, expected);
    QFETCH(bool, knownBug);

    if (knownBug) QEXPECT_FAIL("", "isNow() requires start <= time < end, so a window that crosses midnight is never active", Continue);
    QCOMPARE(dsqt::bridge::isNow(QDateTime(kToday, now), QDateTime(kToday, start), QDateTime(kToday, end)), expected);
}

void DatabaseEventsTest::durationInSeconds_overnight() {
    QCOMPARE(dsqt::bridge::durationInSeconds(at(9), at(17)), 8 * 3600); // The normal case, for reference.

    QEXPECT_FAIL("", "durationInSeconds() uses QTime::secsTo(), which is negative when the end is before the start", Continue);
    QCOMPARE(dsqt::bridge::durationInSeconds(at(22), at(2)), 4 * 3600);
    QEXPECT_FAIL("", "durationInSeconds() uses QTime::secsTo(), which is negative when the end is before the start", Continue);
    QCOMPARE(dsqt::bridge::durationInSeconds(at(22), at(0)), 2 * 3600);
}

void DatabaseEventsTest::eventTimeline_overnightEvent() {
    const auto timeline = eventTimeline(list({makeEvent("night", QTime(22, 0), QTime(2, 0))}), kToday);

    QEXPECT_FAIL("", "eventTimeline() removes events whose end time is before their start time", Continue);
    QVERIFY(!timeline.isEmpty());
}

void DatabaseEventsTest::getCurrentEvent_overnightEvent() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content;
    Access::add(content, makeEvent("night", QTime(22, 0), QTime(2, 0)));

    QVERIFY(content.getCurrentEvent(at(12)).isEmpty()); // Not active during the day.
    QEXPECT_FAIL("", "isNow() never accepts a window that crosses midnight", Continue);
    QCOMPARE(content.getCurrentEvent(at(23)).uid(), QString("night"));
}

// ---- Time zones and daylight saving time ------------------------------------------------------------------------
//
// queryTables() turns the stored date and time into a QDateTime in the local time zone, and the helpers then read
// date() and time() from it: isToday(), isNow(), sortEvents() and durationInSeconds() work on the wall clock.
// isWithinSpan() compares complete QDateTimes, so it works on instants. The tests below pin that down.
//
// The event times are built in UTC, which has no gaps or repeated hours, so they do not depend on the time zone of
// the machine that runs the tests. The "now" values are exact instants, converted to the zone under test.

void DatabaseEventsTest::isNow_readsTheWallClockOfTheGivenDateTime() {
    const QTimeZone plusTwo = fixedOffset(2 * 3600);
    const QDateTime start   = utc(kToday, 9);
    const QDateTime end     = utc(kToday, 17);

    // 10:00 at UTC+2 is the same instant as 08:00 UTC.
    const QDateTime local = utc(kToday, 8).toTimeZone(plusTwo);
    QCOMPARE(local.time(), QTime(10, 0));
    QCOMPARE(local, utc(kToday, 8));

    QVERIFY(dsqt::bridge::isNow(local, start, end));
    // The same instant, expressed in UTC, reads 08:00 and falls outside the 09:00-17:00 window. Callers must pass the
    // local date and time, as the parameter name says. Passing QDateTime::currentDateTimeUtc() gives wrong answers.
    QVERIFY(!dsqt::bridge::isNow(local.toUTC(), start, end));
}

void DatabaseEventsTest::isWithinSpan_comparesInstants() {
    const QTimeZone plusTwo = fixedOffset(2 * 3600);
    // 09:00-17:00 at UTC+2, which is 07:00-15:00 UTC.
    const QDateTime start = utc(kToday, 7).toTimeZone(plusTwo);
    const QDateTime end   = utc(kToday, 15).toTimeZone(plusTwo);
    QCOMPARE(start.time(), QTime(9, 0));
    QCOMPARE(end.time(), QTime(17, 0));

    // 15:30-16:30 UTC reads as inside the event's 09:00-17:00 on the wall clock, but it starts after the event ended.
    QVERIFY(!dsqt::bridge::isWithinSpan(utc(kToday, 15, 30), utc(kToday, 16, 30), start, end));
    // 14:00-15:30 UTC overlaps the last hour of the event.
    QVERIFY(dsqt::bridge::isWithinSpan(utc(kToday, 14), utc(kToday, 15, 30), start, end));
    // The same span in the event's own zone gives the same answer.
    QVERIFY(dsqt::bridge::isWithinSpan(utc(kToday, 14).toTimeZone(plusTwo), utc(kToday, 15, 30).toTimeZone(plusTwo),
                                       start, end));
}

// On 25 October 2026 the clocks in Amsterdam go back from 03:00 CEST to 02:00 CET, so 02:00-03:00 happens twice.
void DatabaseEventsTest::isNow_dstFallBack_bothOccurrencesOfTheRepeatedHour() {
    const QTimeZone amsterdam(QByteArrayLiteral("Europe/Amsterdam"));
    if (!amsterdam.isValid()) QSKIP("The Europe/Amsterdam time zone is not available.");

    const QDate     day(2026, 10, 25);
    const QDateTime start = utc(day, 2); // The window is 02:00-03:00 on the wall clock.
    const QDateTime end   = utc(day, 3);

    const QDateTime firstTime  = utc(day, 0, 30).toTimeZone(amsterdam); // 02:30 CEST
    const QDateTime secondTime = utc(day, 1, 30).toTimeZone(amsterdam); // 02:30 CET
    QCOMPARE(firstTime.time(), QTime(2, 30));
    QCOMPARE(secondTime.time(), QTime(2, 30));
    QCOMPARE(firstTime.offsetFromUtc(), 2 * 3600);
    QCOMPARE(secondTime.offsetFromUtc(), 3600);

    // The event is active during both occurrences, so it lasts two real hours.
    QVERIFY(dsqt::bridge::isNow(firstTime, start, end));
    QVERIFY(dsqt::bridge::isNow(secondTime, start, end));
    // 03:00 CET is when it ends; the window is half-open.
    QVERIFY(!dsqt::bridge::isNow(utc(day, 2).toTimeZone(amsterdam), start, end));
    // 01:59:59 CEST is just before it.
    QVERIFY(!dsqt::bridge::isNow(utc(day, 23, 59, 59).addDays(-1).toTimeZone(amsterdam), start, end));
}

// On 28 March 2027 the clocks in Amsterdam jump from 02:00 CET to 03:00 CEST, so 02:00-03:00 does not exist.
void DatabaseEventsTest::isNow_dstSpringForward_continuesAcrossTheGap() {
    const QTimeZone amsterdam(QByteArrayLiteral("Europe/Amsterdam"));
    if (!amsterdam.isValid()) QSKIP("The Europe/Amsterdam time zone is not available.");

    const QDate     day(2027, 3, 28);
    const QDateTime start = utc(day, 1); // The window is 01:00-04:00 on the wall clock.
    const QDateTime end   = utc(day, 4);

    const QDateTime beforeGap = utc(day, 0, 59, 59).toTimeZone(amsterdam); // 01:59:59 CET
    const QDateTime afterGap  = utc(day, 1).toTimeZone(amsterdam);         // 03:00:00 CEST
    QCOMPARE(beforeGap.time(), QTime(1, 59, 59));
    QCOMPARE(afterGap.time(), QTime(3, 0));
    QCOMPARE(beforeGap.offsetFromUtc(), 3600);
    QCOMPARE(afterGap.offsetFromUtc(), 2 * 3600);

    // The event stays active across the gap, one second of real time later.
    QVERIFY(dsqt::bridge::isNow(beforeGap, start, end));
    QVERIFY(dsqt::bridge::isNow(afterGap, start, end));
    // 04:00 CEST is the end, 00:59:59 CET is before the start.
    QVERIFY(!dsqt::bridge::isNow(utc(day, 2).toTimeZone(amsterdam), start, end));
    QVERIFY(!dsqt::bridge::isNow(utc(day, 0).toTimeZone(amsterdam).addSecs(-1), start, end));
}

// durationInSeconds() subtracts wall-clock times. On a day with a DST change that differs from the real elapsed time.
// The priority rules in sortEvents() only compare durations with each other, so this is harmless there.
void DatabaseEventsTest::durationInSeconds_isWallClockTimeOnDstDays() {
    const QTimeZone amsterdam(QByteArrayLiteral("Europe/Amsterdam"));
    if (!amsterdam.isValid()) QSKIP("The Europe/Amsterdam time zone is not available.");

    const QDate     springDay(2027, 3, 28);
    const QDateTime springStart = utc(springDay, 0).toTimeZone(amsterdam); // 01:00 CET
    const QDateTime springEnd   = utc(springDay, 2).toTimeZone(amsterdam); // 04:00 CEST
    QCOMPARE(springStart.secsTo(springEnd), qint64(2 * 3600));             // Two hours really pass...
    QCOMPARE(dsqt::bridge::durationInSeconds(springStart, springEnd), 3 * 3600); // ...but the clock moves three.

    const QDate     fallDay(2026, 10, 25);
    const QDateTime fallStart = utc(fallDay, 0).toTimeZone(amsterdam); // 02:00 CEST
    const QDateTime fallEnd   = utc(fallDay, 3).toTimeZone(amsterdam); // 04:00 CET
    QCOMPARE(fallStart.secsTo(fallEnd), qint64(3 * 3600));
    QCOMPARE(dsqt::bridge::durationInSeconds(fallStart, fallEnd), 2 * 3600);
}

QTEST_GUILESS_MAIN(DatabaseEventsTest)
#include "tst_databaseevents.moc"
