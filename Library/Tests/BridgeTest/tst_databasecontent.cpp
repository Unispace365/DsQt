#include "testHelpers.h"

#include <QTest>

using namespace dsqt::bridge;
using namespace testing;

namespace {

/// A small tree. Ranks are chosen so that insertion order differs from rank order:
///
///   R (root content, rank 1)
///   ├── A (rank 2)
///   │   ├── A1 (rank 4)
///   │   └── A2 (rank 5)
///   └── B (rank 3)
///   P1, P2 (platforms)        E1, E2 (events)
DatabaseContent makeSample() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content;
    // Deliberately not in rank order.
    Access::add(content, makeRecord("A2", "", 5, {"A"}));
    Access::add(content, makeRecord("B", "", 3, {"R"}));
    Access::add(content, makeRecord("R", "ROOT_CONTENT", 1));
    Access::add(content, makeRecord("A1", "", 4, {"A"}));
    Access::add(content, makeRecord("A", "", 2, {"R"}));
    Access::add(content, makeRecord("P1", "ROOT_PLATFORM", 6, {}, {{"type_name", "Lounge"}}));
    Access::add(content, makeRecord("P2", "ROOT_PLATFORM", 7, {}, {{"type_name", "Portal"}}));
    Access::add(content, makeEvent("E1", QTime(9, 0), QTime(10, 0)));
    Access::add(content, makeEvent("E2", QTime(11, 0), QTime(12, 0)));
    Access::buildTree(content);
    return content;
}

QStringList traverse(DatabaseTree& tree) {
    QStringList result;
    for (const auto& record : tree)
        result.append(record.uid());
    return result;
}

} // namespace

// Tests for DatabaseContent (lookup, grouping, tree building) and DatabaseTree (traversal).
class DatabaseContentTest : public QObject {
    Q_OBJECT

  private slots:
    void emptyContent();

    void find_unknownUidGivesEmptyRecord();
    void find_list_skipsUnknownUidsAndKeepsOrder();
    void groups_followVariant();
    void recordUids_areSortedByRank();

    void buildTree_sortsChildrenByRank();
    void buildTree_ignoresRecordsWithoutExactlyOneKnownParent();
    void buildTree_trimsParentUids();
    void buildTree_canBeRepeated();

    void getPlatform_byTypeName();
    void getPlatform_byTypeName_returnsFirstMatch();

    void tree_preOrder();
    void tree_postOrder();
    void tree_unknownRootIsEmpty();
    void tree_fromContent_startsAtFirstContentRoot();
    void tree_roots_areRecordsWithoutExactlyOneParent();

    void tree_leafRoot();
    void tree_missingChildrenAreSkipped();
    void tree_multiParentRecordIsNotVisited();
    void tree_cyclesAndOrphans_areUnreachableAndNotRoots();
    void tree_roots_afterBuildTree_includeRecordsWithoutParents();
    void buildTree_equalRanksKeepEveryChild();
    void tree_deepChain();
    void tree_wideFanOut();
};

void DatabaseContentTest::emptyContent() {
    DatabaseContent content;
    QVERIFY(content.records().isEmpty());
    QVERIFY(content.recordUids().isEmpty());
    QVERIFY(content.content().isEmpty());
    QVERIFY(content.events().isEmpty());
    QVERIFY(content.platforms().isEmpty());
    QVERIFY(content.find("x").isEmpty());
    QVERIFY(content.find(QStringList{"x", "y"}).isEmpty());
    QVERIFY(content.getPlatform("Lounge").isEmpty());
    QVERIFY(content.getCurrentEvent(at(12)).isEmpty());
}

void DatabaseContentTest::find_unknownUidGivesEmptyRecord() {
    const DatabaseContent content = makeSample();
    QCOMPARE(content.find("A").uid(), QString("A"));
    const DatabaseRecord missing = content.find("nope");
    QVERIFY(missing.isEmpty());
    QCOMPARE(missing.uid(), QString());
}

void DatabaseContentTest::find_list_skipsUnknownUidsAndKeepsOrder() {
    const DatabaseContent content = makeSample();
    QCOMPARE(uids(content.find(QStringList{"B", "nope", "A", "also-nope"})), (QStringList{"B", "A"}));
    QVERIFY(content.find(QStringList()).isEmpty());
}

void DatabaseContentTest::groups_followVariant() {
    const DatabaseContent content = makeSample();
    QCOMPARE(uids(content.content()), (QStringList{"R"}));
    QCOMPARE(uids(content.platforms()), (QStringList{"P1", "P2"}));
    QCOMPARE(uids(content.events()), (QStringList{"E1", "E2"}));
    QCOMPARE(content.contentUids(), (QStringList{"R"}));
    QCOMPARE(content.platformUids(), (QStringList{"P1", "P2"}));
    QCOMPARE(content.eventUids(), (QStringList{"E1", "E2"}));
}

void DatabaseContentTest::recordUids_areSortedByRank() {
    const DatabaseContent content = makeSample();
    // E1 and E2 have rank 0 (makeEvent does not set one), so they come first.
    const QStringList sorted = content.recordUids();
    QCOMPARE(sorted.size(), qsizetype(9));

    const QStringList tail = sorted.mid(2);
    QCOMPARE(tail, (QStringList{"R", "A", "B", "A1", "A2", "P1", "P2"}));

    QStringList head = sorted.mid(0, 2); // The order of equal ranks is not defined.
    head.sort();
    QCOMPARE(head, (QStringList{"E1", "E2"}));
}

void DatabaseContentTest::buildTree_sortsChildrenByRank() {
    const DatabaseContent content = makeSample();
    QCOMPARE(content.find("R").childUids(), (QStringList{"A", "B"}));
    QCOMPARE(content.find("A").childUids(), (QStringList{"A1", "A2"}));
    QVERIFY(content.find("B").childUids().isEmpty());
    QVERIFY(content.find("A1").childUids().isEmpty());
}

void DatabaseContentTest::buildTree_ignoresRecordsWithoutExactlyOneKnownParent() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content;
    Access::add(content, makeRecord("P", "", 1));
    Access::add(content, makeRecord("Q", "", 2));
    Access::add(content, makeRecord("one", "", 3, {"P"}));
    Access::add(content, makeRecord("two", "", 4, {"P", "Q"})); // Several parents: not linked anywhere.
    Access::add(content, makeRecord("none", "", 5));              // No parent.
    Access::add(content, makeRecord("orphan", "", 6, {"gone"}));  // Parent does not exist.
    Access::buildTree(content);

    QCOMPARE(content.find("P").childUids(), (QStringList{"one"}));
    QVERIFY(content.find("Q").childUids().isEmpty());
    // The records themselves are still there.
    QCOMPARE(content.find("two").parentUids(), (QStringList{"P", "Q"}));
    QVERIFY(!content.find("orphan").isEmpty());
}

void DatabaseContentTest::buildTree_trimsParentUids() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content;
    Access::add(content, makeRecord("P", "", 1));
    Access::add(content, makeRecord("c", "", 2, {" P "}));
    Access::buildTree(content);
    QCOMPARE(content.find("P").childUids(), (QStringList{"c"}));
}

void DatabaseContentTest::buildTree_canBeRepeated() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content = makeSample();
    // Stale children are replaced, and linking twice does not create duplicates.
    Access::records(content)["B"]["child_uid"] = QStringList{"stale"};
    Access::buildTree(content);
    Access::buildTree(content);

    QCOMPARE(content.find("R").childUids(), (QStringList{"A", "B"}));
    QCOMPARE(content.find("A").childUids(), (QStringList{"A1", "A2"}));
    QVERIFY(content.find("B").childUids().isEmpty());
}

void DatabaseContentTest::getPlatform_byTypeName() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content = makeSample();
    // A record that is not a platform never matches, even with the same type name.
    Access::add(content, makeRecord("X", "", 8, {}, {{"type_name", "Kiosk"}}));

    QCOMPARE(content.getPlatform("Portal").uid(), QString("P2"));
    QCOMPARE(content.getPlatform("Lounge").uid(), QString("P1"));
    QVERIFY(content.getPlatform("Kiosk").isEmpty());
    QVERIFY(content.getPlatform("Unknown").isEmpty());
}

void DatabaseContentTest::getPlatform_byTypeName_returnsFirstMatch() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content;
    Access::add(content, makeRecord("second", "ROOT_PLATFORM", 2, {}, {{"type_name", "Lounge"}}));
    Access::add(content, makeRecord("first", "ROOT_PLATFORM", 1, {}, {{"type_name", "Lounge"}}));
    // "First" means first in the list of platforms, which is the order in which records were added.
    QCOMPARE(content.getPlatform("Lounge").uid(), QString("second"));
}

void DatabaseContentTest::tree_preOrder() {
    DatabaseContent content = makeSample();
    DatabaseTree    tree(DatabaseContentTestAccess::records(content), "R", DatabaseTree::Traversal::PreOrder);
    QCOMPARE(traverse(tree), (QStringList{"R", "A", "A1", "A2", "B"}));
}

void DatabaseContentTest::tree_postOrder() {
    DatabaseContent content = makeSample();
    DatabaseTree    tree(DatabaseContentTestAccess::records(content), "R", DatabaseTree::Traversal::PostOrder);
    QCOMPARE(traverse(tree), (QStringList{"A1", "A2", "A", "B", "R"}));
}

void DatabaseContentTest::tree_unknownRootIsEmpty() {
    DatabaseContent content = makeSample();
    DatabaseTree    pre(DatabaseContentTestAccess::records(content), "nope", DatabaseTree::Traversal::PreOrder);
    DatabaseTree    post(DatabaseContentTestAccess::records(content), "nope", DatabaseTree::Traversal::PostOrder);
    QVERIFY(pre.begin() == pre.end());
    QVERIFY(post.begin() == post.end());
}

void DatabaseContentTest::tree_fromContent_startsAtFirstContentRoot() {
    DatabaseContent content = makeSample();
    DatabaseTree    tree(content); // Post-order by default.
    QCOMPARE(traverse(tree), (QStringList{"A1", "A2", "A", "B", "R"}));
}

// The "roots" traversal collects records whose parent_uid does not hold exactly one uid. That is what
// DsBridgeSqlQuery uses to find the starting points of the processing queue. Note that a record that has no
// parent_uid at all is not returned; queryTables() only stores parent_uid when a record has at least one parent.
void DatabaseContentTest::tree_roots_areRecordsWithoutExactlyOneParent() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content;
    Access::add(content, makeRecord("single", "", 1, {"p"}));
    Access::add(content, makeRecord("double", "", 2, {"p", "q"}));
    Access::add(content, makeRecord("empty list", "", 3, {}, {{"parent_uid", QStringList()}}));
    Access::add(content, makeRecord("no key", "", 4));

    DatabaseTree tree(Access::records(content), QString(), DatabaseTree::Traversal::Roots);
    QStringList  found = traverse(tree);
    found.sort();
    QCOMPARE(found, (QStringList{"double", "empty list"}));
}

// ---- Odd shapes -------------------------------------------------------------------------------------------------
//
// buildTree() only links a record to its parent when the record has exactly one parent_uid and that parent exists.
// Every record has at most one parent in the tree, so a cycle can never be reached from a root: the traversals
// terminate. (DatabaseTree itself has no guard against a cycle that is wired up by hand through child_uid: a root
// that is its own child would recurse until the stack overflows. buildTree() can produce that only for a root
// record whose parent_uid names itself, so it is not tested here.)

void DatabaseContentTest::tree_leafRoot() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content;
    Access::add(content, makeRecord("R", "ROOT_CONTENT", 1));
    Access::buildTree(content);

    DatabaseTree pre(Access::records(content), "R", DatabaseTree::Traversal::PreOrder);
    DatabaseTree post(Access::records(content), "R", DatabaseTree::Traversal::PostOrder);
    QCOMPARE(traverse(pre), (QStringList{"R"}));
    QCOMPARE(traverse(post), (QStringList{"R"}));
}

void DatabaseContentTest::tree_missingChildrenAreSkipped() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content = makeSample();
    // A child_uid that names a record that no longer exists, in the middle of the list.
    Access::records(content)["R"]["child_uid"] = QStringList{"A", "ghost", "B"};

    DatabaseTree pre(Access::records(content), "R", DatabaseTree::Traversal::PreOrder);
    QCOMPARE(traverse(pre), (QStringList{"R", "A", "A1", "A2", "B"}));
    DatabaseTree post(Access::records(content), "R", DatabaseTree::Traversal::PostOrder);
    QCOMPARE(traverse(post), (QStringList{"A1", "A2", "A", "B", "R"}));
}

// A record with several parents is not linked below any of them, so a traversal never visits it.
void DatabaseContentTest::tree_multiParentRecordIsNotVisited() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content;
    Access::add(content, makeRecord("R", "ROOT_CONTENT", 1));
    Access::add(content, makeRecord("A", "", 2, {"R"}));
    Access::add(content, makeRecord("M", "", 3, {"R", "A"}));
    Access::buildTree(content);

    DatabaseTree pre(Access::records(content), "R", DatabaseTree::Traversal::PreOrder);
    QCOMPARE(traverse(pre), (QStringList{"R", "A"}));
    QCOMPARE(content.find("R").childUids(), (QStringList{"A"}));
    QVERIFY(content.find("A").childUids().isEmpty());
}

// Records that are their own parent, parents of each other, or whose parent does not exist all have exactly one
// parent_uid. They are linked to each other, but nothing leads to them from a root, and they are not roots themselves,
// so they are silently left out of both kinds of traversal.
void DatabaseContentTest::tree_cyclesAndOrphans_areUnreachableAndNotRoots() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content;
    Access::add(content, makeRecord("R", "ROOT_CONTENT", 1));
    Access::add(content, makeRecord("A", "", 2, {"R"}));
    Access::add(content, makeRecord("S", "", 3, {"S"}));  // Its own parent.
    Access::add(content, makeRecord("C1", "", 4, {"C2"})); // A cycle of two.
    Access::add(content, makeRecord("C2", "", 5, {"C1"}));
    Access::add(content, makeRecord("O", "", 6, {"gone"})); // Parent does not exist.
    Access::add(content, makeRecord("Z", "", 7));           // No parent at all.
    Access::buildTree(content);

    QCOMPARE(content.find("S").childUids(), (QStringList{"S"}));
    QCOMPARE(content.find("C1").childUids(), (QStringList{"C2"}));
    QCOMPARE(content.find("C2").childUids(), (QStringList{"C1"}));

    DatabaseTree pre(Access::records(content), "R", DatabaseTree::Traversal::PreOrder);
    QCOMPARE(traverse(pre), (QStringList{"R", "A"}));
    DatabaseTree post(Access::records(content), "R", DatabaseTree::Traversal::PostOrder);
    QCOMPARE(traverse(post), (QStringList{"A", "R"}));

    DatabaseTree roots(Access::records(content), QString(), DatabaseTree::Traversal::Roots);
    QStringList  found = traverse(roots);
    found.sort();
    QCOMPARE(found, (QStringList{"R", "Z"}));
}

// DsBridgeSqlQuery calls buildTree() and then collects the roots. Records without any parent do not have a parent_uid
// when queryTables() creates them, and the "roots" traversal skips records without that key, yet they are found after
// buildTree(). That works because buildTree() reads parent_uid with operator[], which adds an empty entry. This test
// pins down the observable result: after buildTree(), every record without exactly one parent is a root.
void DatabaseContentTest::tree_roots_afterBuildTree_includeRecordsWithoutParents() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content = makeSample();
    Access::add(content, makeRecord("M", "", 8, {"A", "B"})); // Two parents.
    Access::buildTree(content);

    DatabaseTree roots(Access::records(content), QString(), DatabaseTree::Traversal::Roots);
    QStringList  found = traverse(roots);
    found.sort();
    // The root content, both platforms and both events have no parent. M has two.
    QCOMPARE(found, (QStringList{"E1", "E2", "M", "P1", "P2", "R"}));
}

// std::sort is not stable, so the order of children with the same rank is not defined. They must all be there.
void DatabaseContentTest::buildTree_equalRanksKeepEveryChild() {
    using Access = DatabaseContentTestAccess;
    DatabaseContent content;
    Access::add(content, makeRecord("R", "ROOT_CONTENT", 1));
    for (const QString& uid : {"c1", "c2", "c3", "c4"})
        Access::add(content, makeRecord(uid, "", 5, {"R"}));
    Access::buildTree(content);

    QStringList children = content.find("R").childUids();
    QCOMPARE(children.size(), qsizetype(4));
    children.sort();
    QCOMPARE(children, (QStringList{"c1", "c2", "c3", "c4"}));
}

// 500 levels deep. The traversals are recursive, so this guards against a stack that is too small for realistic trees.
void DatabaseContentTest::tree_deepChain() {
    using Access = DatabaseContentTestAccess;
    constexpr int depth = 500;

    DatabaseContent content;
    QStringList     expected;
    for (int i = 0; i < depth; ++i) {
        const QString uid = QString("N%1").arg(i);
        expected.append(uid);
        if (i == 0)
            Access::add(content, makeRecord(uid, "ROOT_CONTENT", i));
        else
            Access::add(content, makeRecord(uid, "", i, {QString("N%1").arg(i - 1)}));
    }
    Access::buildTree(content);

    DatabaseTree pre(Access::records(content), "N0", DatabaseTree::Traversal::PreOrder);
    QCOMPARE(traverse(pre), expected);
    DatabaseTree post(Access::records(content), "N0", DatabaseTree::Traversal::PostOrder);
    QStringList  reversed(expected.crbegin(), expected.crend());
    QCOMPARE(traverse(post), reversed);
}

// One parent with 200 children, added in an order that is unrelated to their ranks. The count is kept modest because
// buildTree() re-sorts a parent's whole child list for every child it adds, which gets slow for large families.
void DatabaseContentTest::tree_wideFanOut() {
    using Access = DatabaseContentTestAccess;
    constexpr int count = 200;

    DatabaseContent content;
    Access::add(content, makeRecord("R", "ROOT_CONTENT", -1));
    QStringList byRank(count);
    for (int i = 0; i < count; ++i) {
        const int     rank = (i * 7919) % count; // 7919 shares no factor with 200: a permutation of 0..199.
        const QString uid  = QString("C%1").arg(i, 3, 10, QChar('0'));
        Access::add(content, makeRecord(uid, "", rank, {"R"}));
        byRank[rank] = uid;
    }
    Access::buildTree(content);

    QCOMPARE(content.find("R").childUids(), byRank);
    DatabaseTree pre(Access::records(content), "R", DatabaseTree::Traversal::PreOrder);
    QCOMPARE(traverse(pre), QStringList("R") + byRank);
}

QTEST_GUILESS_MAIN(DatabaseContentTest)
#include "tst_databasecontent.moc"
