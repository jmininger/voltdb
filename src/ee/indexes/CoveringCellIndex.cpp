/* This file is part of VoltDB.
 * Copyright (C) 2008-2018 VoltDB Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with VoltDB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <tuple>

#include "indexes/CoveringCellIndex.h"

#include "common/GeographyValue.hpp"
#include "common/NValue.hpp"
#include "common/tabletuple.h"
#include "storage/persistenttable.h"

namespace voltdb {

    // This table was generated by the EE GenerateCellLevelInfo in
    // test suite tests/ee/indexes/CoveringCellIndexTest.cpp.
    //
    //    avg area of cells in level  0: 85010980.16 km^2
    //    avg area of cells in level  1: 21252745.04 km^2
    //    avg area of cells in level  2:  5313186.26 km^2
    //    avg area of cells in level  3:  1328296.57 km^2
    //    avg area of cells in level  4:   332074.14 km^2
    //    avg area of cells in level  5:    83018.54 km^2
    //    avg area of cells in level  6:    20754.63 km^2
    //    avg area of cells in level  7:     5188.66 km^2
    //    avg area of cells in level  8:     1297.16 km^2
    //    avg area of cells in level  9:      324.29 km^2
    //    avg area of cells in level 10:       81.07 km^2
    //    avg area of cells in level 11:       20.27 km^2
    //    avg area of cells in level 12:        5.07 km^2
    //    avg area of cells in level 13:        1.27 km^2
    //    avg area of cells in level 14:        0.32 km^2
    //    avg area of cells in level 15:    79172.64 m^2
    //    avg area of cells in level 16:    19793.16 m^2
    //    avg area of cells in level 17:     4948.29 m^2
    //    avg area of cells in level 18:     1237.07 m^2
    //    avg area of cells in level 19:      309.27 m^2
    //    avg area of cells in level 20:       77.32 m^2
    //    avg area of cells in level 21:       19.33 m^2
    //    avg area of cells in level 22:        4.83 m^2
    //    avg area of cells in level 23:        1.21 m^2
    //    avg area of cells in level 24:        0.30 m^2
    //    avg area of cells in level 25:      755.05 cm^2
    //    avg area of cells in level 26:      188.76 cm^2
    //    avg area of cells in level 27:       47.19 cm^2
    //    avg area of cells in level 28:       11.80 cm^2
    //    avg area of cells in level 29:        2.95 cm^2
    //    avg area of cells in level 30:        0.74 cm^2

static const int MIN_CELL_LEVEL = 0;  // entire cube face
static const int MAX_CELL_LEVEL = 16; //
static const int CELL_LEVEL_MOD = 2;  // every other level

static void getCovering(const Polygon &poly, std::vector<S2CellId> *coveringCells) {
    S2RegionCoverer coverer;
    coverer.set_min_level(MIN_CELL_LEVEL);
    coverer.set_max_level(MAX_CELL_LEVEL);
    coverer.set_max_cells(CoveringCellIndex::MAX_CELL_COUNT);
    coverer.set_level_mod(CELL_LEVEL_MOD);
    coverer.GetCovering(poly, coveringCells);
}


static CoveringCellIndex::CellKeyType setKeyFromCellId(uint64_t cellId, const TableTuple* tuple) {
    CoveringCellIndex::CellKeyType key;
    // these two ints cannot be const since callee is expecting a
    // non-const ref.  Typical use is to call this in a loop for
    // multi-component indexes.
    int keyOffset = 0;
    int intraKeyOffset = static_cast<int>(sizeof(uint64_t) - 1);
    key.insertKeyValue<uint64_t>(keyOffset, intraKeyOffset, cellId);
    if (tuple != NULL) {
        key.setValue(tuple->address());
    }

    return key;
}


static CoveringCellIndex::CellKeyType setKeyFromCellId(uint64_t cellId) {
    return setKeyFromCellId(cellId, NULL);
}


static CoveringCellIndex::TupleKeyType setKeyFromTuple(const TableTuple* tuple) {
    CoveringCellIndex::TupleKeyType key;
    int keyOffset = 0;
    int intraKeyOffset = static_cast<int>(sizeof(void*) - 1);
    key.insertKeyValue<uint64_t>(keyOffset, intraKeyOffset, reinterpret_cast<uint64_t>(tuple->address()));
    return key;
}


static uint64_t extractCellId(const CoveringCellIndex::CellKeyType &key) {
    int keyOffset = 0;
    int intraKeyOffset = static_cast<int>(sizeof(uint64_t) - 1);
    return key.extractKeyValue<uint64_t>(keyOffset, intraKeyOffset);
}


static void* extractTupleAddress(const CoveringCellIndex::TupleKeyType &key) {
    int keyOffset = 0;
    int intraKeyOffset = static_cast<int>(sizeof(void*) - 1);
    return reinterpret_cast<void*>(key.extractKeyValue<uint64_t>(keyOffset, intraKeyOffset));
}


static CoveringCellIndex::CellMapIterator& getIterFromCursor(IndexCursor& cursor) {
    return *reinterpret_cast<CoveringCellIndex::CellMapIterator*>(&(cursor.m_keyIter[0]));
}

static CoveringCellIndex::CellMapIterator& getEndIterFromCursor(IndexCursor& cursor) {
    return *reinterpret_cast<CoveringCellIndex::CellMapIterator*>(&(cursor.m_keyEndIter[0]));
}


bool CoveringCellIndex::getPolygonFromTuple(const TableTuple *tuple, Polygon *poly) const {
    NValue nval = tuple->getNValue(m_columnIndex);
    if (! nval.isNull()) {
        const GeographyValue gv = ValuePeeker::peekGeographyValue(nval);
        poly->initFromGeography(gv);
        return true;
    }

    return false;
}


void CoveringCellIndex::addEntryDo(const TableTuple *tuple,
                                   TableTuple *conflictTuple)
{
    Polygon poly;
    if (! getPolygonFromTuple(tuple, &poly)) {
        // Null polygons are not indexed.
        return;
    }

    std::vector<S2CellId> covering;
    getCovering(poly, &covering);

    BOOST_FOREACH(S2CellId &cell, covering) {
        m_cellEntries.insert(setKeyFromCellId(cell.id(), tuple), tuple->address());
    }

    // Now update our tuple map for fast operations.
    TupleValueType cells;
    for (int i = 0; i < MAX_CELL_COUNT; ++i) {
        if (i < covering.size()) {
            cells[i] = covering[i].id();
        }
        else {
            cells[i] = S2CellId::Sentinel().id();
        }
    }

    m_tupleEntries.insert(setKeyFromTuple(tuple), cells);
}

bool CoveringCellIndex::moveToCoveringCell(const TableTuple* searchKey,
                                           IndexCursor &cursor) const
{
    cursor.m_forward = true;

    GeographyPointValue pt = ValuePeeker::peekGeographyPointValue(searchKey->getNValue(0));
    if (pt.isNull()) {
        cursor.m_match.move(NULL);
        return false;
    }

    S2CellId cell = S2CellId::FromPoint(pt.toS2Point());

    // Start at the highest level (smallest cells) and work to larger cells.
    // Going the other way (largest to smallest cells) would require more state,
    // since a cell has just one parent, but 4 children.
    assert (MAX_CELL_LEVEL % CELL_LEVEL_MOD == MIN_CELL_LEVEL % CELL_LEVEL_MOD);
    for (int level = MAX_CELL_LEVEL; level >= MIN_CELL_LEVEL; level -= CELL_LEVEL_MOD) {
        cell = cell.parent(level);

        CellMapRange iterPair = m_cellEntries.equalRange(setKeyFromCellId(cell.id()));

        CellMapIterator &mapIter = getIterFromCursor(cursor);
        CellMapIterator &mapEndIter = getEndIterFromCursor(cursor);

        mapIter = iterPair.first;
        mapEndIter = iterPair.second;

        if (! mapIter.equals(mapEndIter)) {
            cursor.m_match.move(const_cast<void*>(mapIter.value()));
            return true;
        }

        // If no match, we'll try the next level.
    }

    // If we get here there were no matches in any level.
    cursor.m_match.move(NULL);
    return false;
}

TableTuple CoveringCellIndex::nextValueAtKey(IndexCursor& cursor) const
{
    if (cursor.m_match.isNullTuple()) {
        return cursor.m_match;
    }

    TableTuple retval = cursor.m_match;

    CellMapIterator &mapIter = getIterFromCursor(cursor);
    CellMapIterator &mapEndIter = getEndIterFromCursor(cursor);

    S2CellId cell = S2CellId(extractCellId(mapIter.key()));
    int nextLevel = cell.level();

    mapIter.moveNext();
    while (mapIter.equals(mapEndIter)) {
        // No more matches at the current level, but check the lower levels
        // (that is, the cells that contain the one we just checked).
        nextLevel -= CELL_LEVEL_MOD;

        if (nextLevel < MIN_CELL_LEVEL) {
            // No more matches.
            cursor.m_match.move(NULL);
            return retval;
        }
        cell = cell.parent(nextLevel);
        CellMapRange iterPair = m_cellEntries.equalRange(setKeyFromCellId(cell.id()));

        mapIter = iterPair.first;
        mapEndIter = iterPair.second;
    }

    cursor.m_match.move(const_cast<void*>(mapIter.value()));
    return retval;
}


bool CoveringCellIndex::deleteEntryDo(const TableTuple *tuple) {
    std::cout << "CoveringCellIndex::deleteEntryDo: initial tuple data is "
              << (tuple->m_data ? "not " : "")
              << "null.\n";
    NValue nval = tuple->getNValue(m_columnIndex);
    if (nval.isNull()) {
        // null polygons are not indexed.
        return true;
    }

    TupleMapIterator it = m_tupleEntries.find(setKeyFromTuple(tuple));
    if (it.isEnd()) {
        // This tuple was not in our map
        std::cout << "CoveringCellIndex::deleteEntryDo: tuple data at end is "
                  << (tuple->m_data ? "not " : "")
                  << "null.\n";
        return false;
    }

    BOOST_FOREACH(uint64_t cell, it.value()) {
        if (cell == S2CellId::Sentinel().id())
            break;

        CellMapIterator cellIter = m_cellEntries.find(setKeyFromCellId(cell, tuple));
        assert(! cellIter.isEnd());
        m_cellEntries.erase(cellIter);
    }

    std::cout << "CoveringCellIndex::deleteEntryDo: initial tuple data is "
              << (tuple->m_data ? "not " : "")
              << "null.\n";
    m_tupleEntries.erase(it);
    return true;
}

bool CoveringCellIndex::replaceEntryNoKeyChangeDo(const TableTuple &destinationTuple,
                                                  const TableTuple &originalTuple) {
    NValue nval = destinationTuple.getNValue(m_columnIndex);
    if (nval.isNull()) {
        // null polygons are not in the index, so success is doing nothing.
        return true;
    }

    TupleMapIterator tupleMapIt = m_tupleEntries.find(setKeyFromTuple(&originalTuple));
    if (tupleMapIt.isEnd()) {
        return false;
    }

    // We could just call add/delete entry here, but let's avoid
    // having to generate the cell covering again (this is expensive,
    // it takes about half a millisecond).
    TupleValueType cells = tupleMapIt.value();
    for (int i = 0; i < MAX_CELL_COUNT; ++i) {
        if (cells[i] == S2CellId::Sentinel().id()) {
            break;
        }
        CellMapIterator cellMapIt = m_cellEntries.find(setKeyFromCellId(cells[i], &originalTuple));
        assert(! cellMapIt.isEnd());
        m_cellEntries.erase(cellMapIt);
        m_cellEntries.insert(setKeyFromCellId(cells[i], &destinationTuple), destinationTuple.address());
    }

    m_tupleEntries.erase(tupleMapIt);
    m_tupleEntries.insert(setKeyFromTuple(&destinationTuple), cells);
    return true;
}

bool CoveringCellIndex::checkForIndexChangeDo(const TableTuple *lhs, const TableTuple *rhs) const {
    if (lhs == rhs) {
        // Same tuple, no index change necessary
        return false;
    }

    NValue lhsNval = lhs->getNValue(m_columnIndex);
    NValue rhsNval = rhs->getNValue(m_columnIndex);
    if (lhsNval.isNull() && rhsNval.isNull()) {
        // Both values are null.
        return false;
    }

    GeographyValue lhsGv = ValuePeeker::peekGeographyValue(lhsNval);
    GeographyValue rhsGv = ValuePeeker::peekGeographyValue(rhsNval);

    if (lhsGv.data() == rhsGv.data()) {
        // different tuples but same geography.
        return false;
    }

    return true;
}


bool CoveringCellIndex::checkValidityForTest(PersistentTable* table, std::string* reasonInvalid) const {

    // Make sure that each row in the table has a matching entry in the tuple map.
    TableIterator tableIt = table->iterator();
    TableTuple tuple(table->schema());
    while (tableIt.next(tuple)) {
        NValue nval = tuple.getNValue(m_columnIndex);
        bool isNull = nval.isNull();

        TupleMapIterator tupleMapIt = m_tupleEntries.find(setKeyFromTuple(&tuple));
        if (tupleMapIt.isEnd()) {
            if (! isNull) {
                *reasonInvalid = "Found non-null polygon not in tuple map";
                return false;
            }
        }
    }

    // Make sure that each entry in the tuple map is a valid row in the table,
    //   and has entries in the cell map for each cell.
    int tupleCount = 0;
    TupleMapIterator tupleMapIt = m_tupleEntries.begin();
    while (!tupleMapIt.isEnd()) {
        void* tupleAddress = extractTupleAddress(tupleMapIt.key());
        TupleValueType cells = tupleMapIt.value();
        int i = 0;
        for (; i < MAX_CELL_COUNT; ++i) {
            if (cells[i] == S2CellId::Sentinel().id()) {
                if (i == 0) {
                    *reasonInvalid = "Should have at least one valid cell";
                    return false;
                }
                break;
            }

            tuple.move(tupleAddress);
            CellMapIterator cellIter = m_cellEntries.find(setKeyFromCellId(cells[i], &tuple));
            if (cellIter.isEnd()) {
                std::ostringstream oss;
                oss << "Could not find cell entry for existing "
                    << tupleCount << "-th tuple at cell " << i;
                *reasonInvalid = oss.str();
                return false;
            }

            if (cellIter.value() != tupleAddress) {
                *reasonInvalid = "Value in cell map entry doesn't match expected";
                return false;
            }
        }

        // Remaining cells should be empty.
        for (; i < MAX_CELL_COUNT; ++i) {
            if (cells[i] != S2CellId::Sentinel().id()) {
                *reasonInvalid = "Found non-sentinel cell after sentinal indicating end of cells";
                return false;
            }

            CellMapIterator cellIter = m_cellEntries.find(setKeyFromCellId(cells[i], &tuple));
            if (! cellIter.isEnd()) {
                *reasonInvalid = "Found sentinel cell in cell map";
                return false;
            }
        }

        ++tupleCount;
        tupleMapIt.moveNext();
    }

    // Make sure that each entry in the cell map corresponds to the tuple map
    CellMapIterator cellMapIt = m_cellEntries.begin();
    while (!cellMapIt.isEnd()) {
        uint64_t cell = extractCellId(cellMapIt.key());
        void* tupleAddress = const_cast<void*>(cellMapIt.value());

        tuple.move(tupleAddress);
        TupleMapIterator tupleMapIt = m_tupleEntries.find(setKeyFromTuple(&tuple));
        if (tupleMapIt.isEnd()) {
            *reasonInvalid = "Did not find tuple from cell map in tuple map";
            return false;
        }
        else {
            TupleValueType cells = tupleMapIt.value();
            bool foundCell = false;
            for (int i = 0; i < MAX_CELL_COUNT; ++i) {
                if (cells[i] == cell) {
                    foundCell = true;
                    break;
                }
            }

            if (!foundCell) {
                *reasonInvalid = "Did not fit cell from cell map in tuple map";
                return false;
            }
        }

        cellMapIt.moveNext();
    }

    return true;
}

CoveringCellIndex::StatsForTest CoveringCellIndex::getStatsForTest(PersistentTable *table) const {
    const double RADIUS_SQ_M = 6371008.8 * 6371008.8; // from geofunctions.cpp
    StatsForTest stats;

    stats.numPolygons = m_tupleEntries.size();
    stats.numCells = m_cellEntries.size();

    // Find the total area of all the cells
    CellMapIterator cellIt = m_cellEntries.begin();
    while (! cellIt.isEnd()) {
        S2Cell cell(S2CellId(extractCellId(cellIt.key())));
        S2Polygon poly(cell);

        double cellArea = RADIUS_SQ_M * poly.GetArea();
        stats.cellsArea += cellArea;

        cellIt.moveNext();
    }

    // Find the total area of all the polygons
    TableTuple tuple(table->schema());
    TupleMapIterator polyIt = m_tupleEntries.begin();
    while (! polyIt.isEnd()) {
        tuple.move(extractTupleAddress(polyIt.key()));
        Polygon poly;
        getPolygonFromTuple(&tuple, &poly);

        double polyArea = RADIUS_SQ_M * poly.GetArea();
        stats.polygonsArea += polyArea;

        polyIt.moveNext();
    }

    return stats;
}

CoveringCellIndex::~CoveringCellIndex()
{
}

} // end namespace voltdb
