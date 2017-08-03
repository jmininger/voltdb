/* This file is part of VoltDB.
 * Copyright (C) 2008-2017 VoltDB Inc.
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

package org.voltdb.iv2;

import org.json_voltpatches.JSONException;
import org.json_voltpatches.JSONObject;
import org.json_voltpatches.JSONStringer;
import com.google_voltpatches.common.base.Charsets;

public class BalanceSpiInfo {

    private int m_oldLeaderHostId = Integer.MIN_VALUE;
    private int m_newLeaderHostId = Integer.MIN_VALUE;
    private long m_oldLeaderHsid = Long.MIN_VALUE;
    private long m_newLeaderHsid = Long.MIN_VALUE;
    private int m_partitionId = Integer.MIN_VALUE;

    public BalanceSpiInfo() { }

    public BalanceSpiInfo(byte[] data) throws JSONException {
        JSONObject jsObj = new JSONObject(new String(data, Charsets.UTF_8));
        m_oldLeaderHostId = jsObj.getInt("oldHostId");
        m_newLeaderHostId = jsObj.getInt("newHostId");
        m_oldLeaderHsid = jsObj.getLong("oldHsid");
        m_newLeaderHsid = jsObj.getLong("newHsid");
        m_partitionId = jsObj.getInt("partitionId");
    }

    public BalanceSpiInfo(int oldHostId, int newHostId, long oldHsid, long newHsid, int partitionId) {
        m_oldLeaderHostId = oldHostId;
        m_newLeaderHostId = newHostId;
        m_oldLeaderHsid = oldHsid;
        m_newLeaderHsid = newHsid;
        m_partitionId = partitionId;
    }

    public byte[] toBytes() throws JSONException {
        JSONStringer js = new JSONStringer();
        js.object();
        js.key("oldHostId").value(m_oldLeaderHostId);
        js.key("newHostId").value(m_newLeaderHostId);
        js.key("oldHsid").value(m_oldLeaderHsid);
        js.key("newHsid").value(m_newLeaderHsid);
        js.key("partitionId").value(m_partitionId);
        js.endObject();
        return js.toString().getBytes(Charsets.UTF_8);
    }

    public long getOldLeaderHsid() {
        return m_oldLeaderHsid;
    }

    public long getNewLeaderHsid() {
        return m_newLeaderHsid;
    }

    public int getOldLeaderHostId() {
        return m_oldLeaderHostId;
    }

    public int getNewLeaderHostId() {
        return m_newLeaderHostId;
    }

    public int getPartitionId() {
        return m_partitionId;
    }
}