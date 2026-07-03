// Design Ref: §3.1, §5.2 — BindingList wrapper for DataGridView two-way binding + order helpers.
using System.ComponentModel;

namespace IntegrateController.Models;

public sealed class DeviceList : BindingList<DeviceEntry>
{
    public DeviceList() { }
    public DeviceList(IEnumerable<DeviceEntry> source) : base(source.ToList()) { }

    public void MoveUp(int idx)
    {
        if (idx <= 0 || idx >= Count) return;
        var item = this[idx];
        RemoveAt(idx);
        Insert(idx - 1, item);
        ReassignOrder();
    }

    public void MoveDown(int idx)
    {
        if (idx < 0 || idx >= Count - 1) return;
        var item = this[idx];
        RemoveAt(idx);
        Insert(idx + 1, item);
        ReassignOrder();
    }

    public void ReassignOrder()
    {
        for (int i = 0; i < Count; i++) this[i].Order = i;
    }

    public DeviceEntry? FindById(string id) =>
        this.FirstOrDefault(d => string.Equals(d.Id, id, StringComparison.Ordinal));
}
