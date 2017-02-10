#include <cstdlib>
#include <algorithm>
#include "imwidget/map_command.h"

#include "imwidget/error_dialog.h"
#include "imwidget/imutil.h"
#include "imwidget/overworld_encounters.h"
#include "imwidget/simplemap.h"
#include "imgui.h"
#include "nes/enemylist.h"
#include "util/config.h"

#include "IconsFontAwesome.h"

namespace z2util {

const DecompressInfo* MapCommand::info_[NR_AREAS][NR_SETS][16];
const char* MapCommand::object_names_[NR_AREAS][NR_SETS][16];
void MapCommand::Init() {
    static bool done;
    static char names[NR_AREAS][NR_SETS][16][64];
    if (done)
        return;
    memset(info_, 0, sizeof(info_));
    memset(object_names_, 0, sizeof(object_names_));
    for(int i=0; i<NR_AREAS; i++) {
        for(int j=0; j<NR_SETS; j++) {
            for(int k=0; k<16; k++) {
                if (j==0 && k==15) {
                    sprintf(names[i][j][k], "%02x: collectable",
                            (j==0 || j==3)? k : k<<4);
                } else {
                    sprintf(names[i][j][k], "%02x: ???",
                            (j==0 || j==3)? k : k<<4);
                }
                object_names_[i][j][k] = names[i][j][k];
            }
        }
    }
    const auto& ri = ConfigLoader<RomInfo>::GetConfig();
    for(const auto& d : ri.decompress()) {
        int id = d.id();
        int val = id;
        if (id & 0xF0) {
            id >>= 4;
        }
        snprintf(names[d.area()][d.type()][id], 63,
                 "%02x: %s", val, d.comment().c_str());
        info_[d.area()][d.type()][id] = &d;
    }
    done = true;
}

MapCommand::MapCommand(const MapHolder* holder, uint8_t position,
                       uint8_t object, uint8_t extra)
  : id_(UniqueID()),
    holder_(holder),
    position_(position),
    object_(object),
    extra_(extra)
{
    data_.x = position_ & 0xf;
    data_.y = position_ >> 4;
    Init();
}

MapCommand::MapCommand(const MapHolder* holder, int x0, uint8_t position,
                       uint8_t object, uint8_t extra)
  : MapCommand(holder, position, object, extra)
{
    if (data_.y == 14) {
        data_.absx = data_.x * 16;
    } else {
        data_.absx = data_.x + x0;
    }
}

/*
MapCommand::MapCommand(MapCommand&& other)
  : id_(other.id_),
    holder_(other.holder_),
    position_(other.position_),
    object_(other.object_),
    extra_(other.extra_),
    data_(other.data_) {}
    */


bool MapCommand::Draw(bool abscoord) {
    bool changed = false;
    const char *xpos = "x position";
    const char *ypos = "y position";
    if (data_.y == 13) {
        ypos = "new floor ";
    } else if (data_.y == 14) {
        ypos = "x skip    ";
        xpos = "to screen#";
    } else if (data_.y == 15) {
        ypos = "extra obj ";
    }

    ImGui::PushID(id_);
    ImGui::PushItemWidth(100);
    changed |= ImGui::InputInt(ypos, &data_.y);
    Clamp(&data_.y, 0, 15);

    ImGui::SameLine();
    if (data_.y != 14 && abscoord) {
        changed |= ImGui::InputInt(xpos, &data_.absx);
        Clamp(&data_.absx, 0, 63);
    } else {
        changed |= ImGui::InputInt(xpos, &data_.x);
        Clamp(&data_.x, 0, 15);
    }

    if (data_.y == 13 || data_.y == 14) {
        sprintf(obuf_, "%02x", object_);
        ImGui::SameLine();
        changed |= ImGui::InputText("param", obuf_, sizeof(obuf_),
                                    ImGuiInputTextFlags_CharsHexadecimal |
                                    ImGuiInputTextFlags_EnterReturnsTrue);
        object_ = strtoul(obuf_, 0, 16);
    } else {
        int n = 0;
        int large_start = 0;
        int extra_start = 0;
        int extra_small_start = 0;
        int type = holder_->map().type();
        int oindex = 1 + !!(holder_->flags() & 0x80);

        for(int i=0; i<NR_SETS; i++) {
            if (i==1) large_start = n;
            if (i==3) extra_small_start = n;
            if (i==4) extra_start = n;
            if ((i==1 || i==2) && i != oindex)
                continue;
            for(int j=0; j<16; j++) {
                names_[n++] = object_names_[type][i][j];
            }
        }

        if (data_.y == 15) {
            if ((object_ & 0xF0) == 0) {
                oindex = 3;
                data_.object = extra_small_start + object_;
                data_.param = 0;
            } else {
                oindex = 4;
                data_.object = extra_start + (object_ >> 4);
                data_.param = object_ & 0x0F;
            }
        } else if ((object_ & 0xF0) == 0) {
            oindex = 0;
            data_.object = object_ & 0x0F;
            data_.param = 0;
        } else {
            data_.object = large_start + (object_ >> 4);
            data_.param = object_ & 0x0F;
        }

        ImGui::PushItemWidth(200);
        ImGui::SameLine();
        changed |= ImGui::Combo("id", &data_.object, names_, n);
        ImGui::PopItemWidth();
        if (changed) {
            printf("changed to %d (%02x) %s\n", data_.object, data_.object,
                    names_[data_.object]);
        }

        // All of the object names start with their ID in hex, so we just
        // parse the value out of the name.
        object_ = strtoul(names_[data_.object], 0, 16);

        // If the index from the combobox is >= the "extra" items, then
        // set y to the magic 'extra items' value.
        if (data_.object >= extra_small_start) {
            data_.y = 15;
        } else if (data_.object < extra_small_start && data_.y == 15) {
            data_.y = 12;
        }
        if (oindex !=0 && oindex != 3) {
            ImGui::SameLine();
            changed |= ImGui::InputInt("param", &data_.param);
            Clamp(&data_.param, 0, 15);
            object_ |= data_.param;
        } else if (object_ == 15) {
            ImGui::SameLine();
            sprintf(ebuf_, "%02x", extra_);
            ImGui::SameLine();
            changed |= ImGui::InputText("collectable", ebuf_, sizeof(ebuf_),
                                        ImGuiInputTextFlags_CharsHexadecimal |
                                        ImGuiInputTextFlags_EnterReturnsTrue);
            extra_ = strtoul(ebuf_, 0, 16);
        }
    }
    ImGui::PopItemWidth();
    ImGui::PopID();

    return changed;
}

std::vector<uint8_t> MapCommand::Command() {
    position_ = (data_.y << 4) | data_.x;
    if (data_.y < 13 && object_ == 15) {
        return {position_, object_, extra_};
    }
    return {position_, object_};
}


MapHolder::MapHolder() {}

const char* ground_names[] = {
    "TileSet (cave)",
    "TileSet (forest)",
    "TileSet (lava)",
    "TileSet (swamp)",
    "TileSet (sand)",
    "TileSet (volcano)",
    "TileSet (north castle)",
    "TileSet (outside)",
};

void MapHolder::Unpack() {
    data_.objset = !!(flags_ & 0x80);
    data_.width = 1 + ((flags_ >> 5) & 3);
    data_.grass = !!(flags_ & 0x10);
    data_.bushes = !!(flags_ & 0x08);
    data_.ceiling = !(ground_ & 0x80);
    data_.ground = (ground_ >> 4) & 7;
    data_.floor = ground_ & 0xf;
    data_.spal = (back_ >> 6) & 3;
    data_.bpal = (back_ >> 3) & 7;
    data_.bmap = back_ & 7;
}

void MapHolder::Pack() {
    flags_ = (data_.objset << 7) |
             ((data_.width-1) << 5) |
             (int(data_.grass) << 4) |
             (int(data_.bushes) << 3);
    ground_ = (int(!data_.ceiling) << 7) | (data_.ground << 4) | (data_.floor);
    back_ = (data_.spal << 6) | (data_.bpal << 3) | data_.bmap;
}

bool MapHolder::Draw() {
    char abuf[8];
    bool changed = false, achanged=false;
    Unpack();

    Address addr = mapper_->ReadAddr(map_.pointer(), 0);
    ImGui::Text("Map pointer at bank=0x%x address=0x%04x",
                map_.pointer().bank(), map_.pointer().address());

    ImGui::AlignFirstTextHeightToWidgets();
    ImGui::Text("Map address at bank=0x%x address=",
                addr.bank());

    ImGui::PushItemWidth(100);
    sprintf(abuf, "%04x", map_addr_);
    ImGui::SameLine();
    achanged = ImGui::InputText("##addr", abuf, 5,
                                ImGuiInputTextFlags_CharsHexadecimal |
                                ImGuiInputTextFlags_EnterReturnsTrue);
    if (achanged) {
        addr_changed_ |= achanged;
        map_addr_ = strtoul(abuf, 0, 16);
        Parse(map_, map_addr_);
        return true;
    }

    ImGui::Text("Length = %d bytes.", length_);

    ImGui::Text("Flags:");
    changed |= ImGui::InputInt("Object Set", &data_.objset);
    Clamp(&data_.objset, 0, 1);

    ImGui::SameLine();
    changed |= ImGui::InputInt("Width", &data_.width);
    Clamp(&data_.width, 1, 4);

    ImGui::SameLine();
    changed |= ImGui::Checkbox("Grass", &data_.grass);

    ImGui::SameLine();
    changed |= ImGui::Checkbox("Bushes", &data_.bushes);

    ImGui::SameLine();
    changed |= ImGui::Checkbox("Cursor Moves Left", &cursor_moves_left_);

    ImGui::Text("Ground:");
    changed |= ImGui::Checkbox("Ceiling", &data_.ceiling);

    ImGui::SameLine();
    changed |= ImGui::InputInt(ground_names[data_.ground], &data_.ground);
    Clamp(&data_.ground, 0, 7);

    ImGui::SameLine();
    changed |= ImGui::InputInt("Floor", &data_.floor);
    Clamp(&data_.floor, 0, 15);

    ImGui::Text("Background:");
    changed |= ImGui::InputInt("Spr Palette", &data_.spal);
    Clamp(&data_.spal, 0, 3);

    ImGui::SameLine();
    changed |= ImGui::InputInt("BG Palette", &data_.bpal);
    Clamp(&data_.bpal, 0, 7);

    ImGui::SameLine();
    changed |= ImGui::InputInt("BG Map", &data_.bmap);
    Clamp(&data_.bmap, 0, 7);

    ImGui::Text("Command List:");
    ImGui::BeginChild("commands", ImGui::GetContentRegionAvail(), true);

    int i = 0, lastx = 0;;
    for(auto it = command_.begin(); it < command_.end(); ++it, ++i) {
        auto next = it + 1;
        bool create = false;
        lastx = it->absx();
        ImGui::PushID(i);
        if (ImGui::Button(ICON_FA_CARET_SQUARE_O_UP)) {
            changed = true;
            create = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Insert a new command");

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_CARET_DOWN)) {
            changed = true;
            std::swap(*it, *next);
        }
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Move command down");

        ImGui::SameLine();
        changed |= it->Draw(true);

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TIMES_CIRCLE)) {
            changed = true;
            command_.erase(it);
        }
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Delete this command");
        ImGui::PopID();
        if (create)
            command_.emplace(it, this, it->absx(), 0, 0, 0);
    }
    if (ImGui::Button(ICON_FA_CARET_SQUARE_O_UP)) {
        changed = true;
        command_.emplace_back(this, lastx, 0, 0, 0);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Append a new command");

    ImGui::EndChild();
    ImGui::PopItemWidth();
    Pack();
    data_changed_ |= changed;
    return changed;
}

void MapHolder::Parse(const z2util::Map& map, uint16_t altaddr) {
    map_ = map;
    // For side view maps, the map address is the address of a pointer
    // to the real address.  Read it and set the real address.
    Address address = mapper_->ReadAddr(map.pointer(), 0);
    if (altaddr) {
        address.set_address(altaddr);
    }
    *map_.mutable_address() = address;
    map_addr_ = address.address();

    length_ = mapper_->Read(address, 0);
    flags_ = mapper_->Read(address, 1);
    ground_ = mapper_->Read(address, 2);
    back_ = mapper_->Read(address, 3);

    command_.clear();
    int absx = 0;
    for(int i=4; i<length_; i+=2) {
        uint8_t pos = mapper_->Read(address, i);
        uint8_t obj = mapper_->Read(address, i+1);
        uint8_t extra = 0;

        int y = pos >> 4;
        if (y < 13 && obj == 15) {
            i++;
            extra = mapper_->Read(address, i+1);
        }
        command_.emplace_back(this, absx, pos, obj, extra);
        absx = command_.back().absx();
    }
}

std::vector<uint8_t> MapHolder::MapDataWorker(std::vector<MapCommand>& list) {
    std::vector<uint8_t> map = {length_, flags_, ground_, back_};
    for(auto& cmd : list) {
        auto bytes = cmd.Command();
        map.insert(map.end(), bytes.begin(), bytes.end());
        LOG(INFO, "CMD: op = ", HEX(bytes[0]), " ", HEX(bytes[1]));
    }
    map[0] = map.size();
    return map;
}

std::vector<uint8_t> MapHolder::MapData() {
    return MapDataWorker(command_);
}

std::vector<uint8_t> MapHolder::MapDataAbs() {
    std::vector<MapCommand> copy = command_;
    if (!cursor_moves_left_) {
        std::stable_sort(copy.begin(), copy.end(),
            [](const MapCommand& a, const MapCommand& b) {
                return a.absx() < b.absx();
            });
    }
    int x = 0;
    for(auto it = copy.begin(); it < copy.end(); ++it) {
        if (it->absy() == 14) {
            // Erase any "skip" commands, as we'll re-synthesize them
            // as needed.
            it = copy.erase(it);
            if (it == copy.end())
                break;
        }
        int deltax = it->absx() - x;
        if (deltax < 0 || deltax > 15) {
            int nx = it->absx() & ~15;
            it = copy.emplace(it, this, x, 0xE0 | (nx/16), 0, 0);
            it++;
            x = nx;
            deltax = it->absx() - x;
        }
        it->set_relx(deltax);
        x = it->absx();
    }

    return MapDataWorker(copy);
}

void MapHolder::Save() {
    if (addr_changed_ && !data_changed_) {
        mapper_->WriteWord(map_.pointer(), 0, map_addr_);
        addr_changed_ = false;
        return;
    }
    if (addr_changed_ && data_changed_) {
        ErrorDialog::New("Unexpected Change When Saving Map",
            "Both the map data and map address have been changed.\n"
            "Allocating a new address and saving data.\n");
    }
    std::vector<uint8_t> data = MapDataAbs();
    LOG(INFO, "Saving ", map_.name(), " (", data.size(), " bytes)");

    Address addr = map_.address();
    // Search the entire bank and allocate memory
    addr.set_address(0);
    addr = mapper_->Alloc(addr, data.size());
    if (addr.address() == 0) {
        ErrorDialog::New("Error Saving Map",
            "Can't save map: ", map_.name(), "\n\n"
            "Can't find ", data.size(), " free bytes in bank ", addr.bank());
        LOG(ERROR, "Can't save map: can't find ", data.size(), "bytes"
                   " in bank=", addr.bank());
        return;
    }
    addr.set_address(0x8000 | addr.address());

    // Free the existing memory if it was owned by the allocator.
    mapper_->Free(map_.address());
    *map_.mutable_address() = addr;
    map_addr_ = addr.address();

    LOG(INFO, "Saving map to offset ", HEX(addr.address()),
              " in bank=", addr.bank());

    for(unsigned i=0; i<data.size(); i++) {
        mapper_->Write(addr, i, data[i]);
    }
    mapper_->WriteWord(map_.pointer(), 0, addr.address());
    data_changed_ = false;
}

MapConnection::MapConnection()
  : mapper_(nullptr)
{}

void MapConnection::Parse(const Map& map) {
    uint8_t val;
    connector_ = map.connector();
    world_ = map.world();
    overworld_ = map.overworld();
    subworld_ = map.subworld();

    for(int i=0; i<4; i++) {
        val = mapper_->Read(connector_, i);
        data_[i].destination = val >> 2;
        data_[i].start = val & 3;
    }
}

void MapConnection::Save() {
    for(int i=0; i<4; i++) {
        uint8_t val = (data_[i].destination << 2) | (data_[i].start & 3);
        mapper_->Write(connector_, i, val);
    }
}

bool MapConnection::Draw() {
    const char *destlabel[] = {
        "Left Exit ",
        "Down Exit ",
        "Up Exit   ",
        "Right Exit",
    };
    const char *startlabel[] = {
        "Left Dest Screen ",
        "Down Dest Screen ",
        "Up Dest Screen   ",
        "Right Dest Screen",
    };
    const char *buttonlabel[] = {
        "View Area##0", "View Area##1", "View Area##2", "View Area##3",
    };
    const char *selection = "0\0001\0002\0003\0\0";
    const auto& ri = ConfigLoader<RomInfo>::GetConfig();
    const char *names[ri.map().size() + 1];
    const Map *maps[ri.map().size() + 1];
    int len = 0;
    bool chg = false;

    for(const auto& m : ri.map()) {
        if (m.type() != MapType::OVERWORLD
            && m.world() == world_
            && m.overworld() == overworld_
            && m.subworld() == subworld_) {
            names[len] = m.name().c_str();
            maps[len] = &m;
            len++;
        }
    }
    names[len++] = "Outside";

    ImGui::Text("Map exit table at bank=0x%x address=0x%04x",
            connector_.bank(), connector_.address());
    for(int i=0; i<4; i++) {
        ImGui::PushItemWidth(400);
        chg |= ImGui::Combo(destlabel[i], &data_[i].destination, names, len);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(100);
        chg |= ImGui::Combo(startlabel[i], &data_[i].start, selection);
        if (data_[i].destination != len-1) {
            ImGui::SameLine();
            if (ImGui::Button(buttonlabel[i])) {
                SimpleMap::New(mapper_, *maps[data_[i].destination]);
            }
        }
        ImGui::PopItemWidth();
    }
    return chg;
}

MapEnemyList::MapEnemyList(Mapper* m)
  : mapper_(m),
    is_large_(false),
    is_encounter_(false),
    world_(0),
    overworld_(0),
    area_(0),
    display_(0) {}

MapEnemyList::MapEnemyList() : MapEnemyList(nullptr) {}

void MapEnemyList::Parse(const Map& map) {
    pointer_ = map.pointer();
    world_ = map.world();
    area_ = map.area();

    /*
    // FIXME(cfrantz): Find a better way of learning the area number
    int a = pointer_.address() | 0x8000;
    if (a >= 0xA000) {
        area_ = (a - 0xA000) / 2;
    } else {
        area_ = (a - 0x8523) / 2;
    }
    */

    // Check if this is an overworld random encounter area
    OverworldEncounters enc;
    enc.set_mapper(mapper_);
    enc.set_map(map);
    enc.Unpack();
    is_encounter_ = enc.IsEncounter(area_);

    Address addr = mapper_->ReadAddr(pointer_, 0x7e);
    uint16_t delta = 0x18a0;

    data_.clear();
    uint8_t n = mapper_->Read(addr, delta);
    if (is_large_) {
        delta += n;
        n = mapper_->Read(addr, delta);
    }
    for(int i=1; i<n; i+=2) {
        uint8_t pos = mapper_->Read(addr, delta+i);
        uint8_t enemy = mapper_->Read(addr, delta+i+1);
        int y = pos >> 4;
        y = (y == 0) ? 1 : y+2;
        data_.emplace_back(enemy & 0x3f,
                           (pos & 0xf) | (enemy & 0xc0) >> 2, y);
    }

    // Encounters have 2 enemy lists, so make another widget
    large_.reset(nullptr);
    if (is_encounter_ && !is_large_) {
        large_.reset(new MapEnemyList(mapper_));
        large_->is_large_ = true;
        large_->Parse(map);
    }
}

std::vector<uint8_t> MapEnemyList::Pack() {
    std::vector<uint8_t> packed;

    uint8_t n = 1 + data_.size() * 2;
    packed.push_back(n);
    for(const auto& data : data_) {
        int y = data.y;
        y = (y <= 1) ? 0 : y-2;
        uint8_t pos = (y << 4) | (data.x & 0x0F);
        uint8_t enemy = (data.enemy & 0x3f) | (data.x & 0x30) << 2;
        packed.push_back(pos);
        packed.push_back(enemy);
    }
    return packed;
}

void MapEnemyList::Save() {
    EnemyListPack ep(mapper_);
    ep.Unpack(pointer_.bank());
    auto data = Pack();
    if (large_) {
        auto more = large_->Pack();
        data.insert(data.end(), more.begin(), more.end());
    }
    ep.Add(area_, data);
    ep.Pack();
}

bool MapEnemyList::Draw() {
    const char *names[256];
    const auto& ri = ConfigLoader<RomInfo>::GetConfig();
    bool chg = false;

    for(int i=0; i<256; i++)
        names[i] = "???";

    int n = 0;
    for(const auto& e : ri.enemies()) {
        if (e.world() == world_ && e.overworld() == overworld_) {
            for(const auto& info : e.info()) {
                names[info.first] = info.second.name().c_str();
                if (info.first >= n)
                    n = info.first+1;
            }
        }
    }

    Address addr = mapper_->ReadAddr(pointer_, 0x7e);
    ImGui::Text("Map enemy table pointer at bank=0x%x address=0x%04x",
                pointer_.bank(), pointer_.address() + 0x7e);
    ImGui::Text("Map enemy table address at bank=0x%x address=0x%04x",
                addr.bank(), addr.address() + 0x18a0);

    if (is_encounter_) {
        if (is_large_) {
            ImGui::RadioButton("Large Enemy Encounter", &display_, 1);
        } else {
            ImGui::RadioButton("Small Enemy Encounter", &large_->display_, 0);
        }
    }

    int i=0;
    for(auto it=data_.begin(); it<data_.end(); ++it, ++i) {
        ImGui::PushID(i | (is_large_ << 8));
        ImGui::PushItemWidth(100);
        chg |= ImGui::InputInt("x position", &it->x);
        ImGui::SameLine();
        chg |= ImGui::InputInt("y position", &it->y);
        ImGui::SameLine();
        ImGui::PopItemWidth();
        ImGui::PushItemWidth(400);
        chg |= ImGui::Combo("enemy", &it->enemy, names, n);
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TIMES_CIRCLE)) {
            chg = true;
            data_.erase(it);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Delete this enemy");
        ImGui::PopID();
    }
    if (ImGui::Button(ICON_FA_CARET_SQUARE_O_UP)) {
        chg = true;
        data_.emplace_back(0, 0, 0);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Add a new enemy");

    if (large_) {
        ImGui::Separator();
        chg |= large_->Draw();
    }
    return chg;
}

const std::vector<MapEnemyList::Unpacked>& MapEnemyList::data() {
    return (large_ && large_->display_) ? large_->data_ : data_;
}


void MapItemAvailable::Parse(const Map& map) {
    const auto& ri = ConfigLoader<RomInfo>::GetConfig();

    area_ = map.area();
    for(const auto& a : ri.available()) {
        if (map.world() == a.world()
            && map.overworld() == a.overworld()
            && map.subworld() == a.subworld()) {
            avail_ = a;
        }
    }

    uint8_t a = mapper_->Read(avail_.address(), area_ / 2);
    for(int i=0; i<4; i++) {
        int bit = 7 - (i + 4 * (area_ & 1));
        data_.avail[i] = !!(a & (1 << bit));
    }
}

void MapItemAvailable::Save() {
    uint8_t a = mapper_->Read(avail_.address(), area_ / 2);
    for(int i=0; i<4; i++) {
        int bit = 7 - (i + 4 * (area_ & 1));
        a &= ~(1 << bit);
        a |= data_.avail[i] << bit;
    }
    mapper_->Write(avail_.address(), area_ / 2, a);
}

bool MapItemAvailable::Draw() {
    bool chg;
    const char* names[] = {
        "Screen 1",
        "Screen 2",
        "Screen 3",
        "Screen 4",
    };
    for(int i=0; i<4; i++) {
        chg |= ImGui::Checkbox(names[i], &data_.avail[i]);
    }
    return chg;
}

bool MapSwapper::Draw() {
    const auto& ri = ConfigLoader<RomInfo>::GetConfig();
    const char *names[ri.map().size() + 1];
    int len = 0;
    bool chg = false;

    for(const auto& m : ri.map()) {
        if (m.type() != MapType::OVERWORLD
            && m.world() == map_.world()
            && m.overworld() == map_.overworld()
            && m.subworld() == map_.subworld()) {
            names[len] = m.name().c_str();
            len++;
        }
    }

    ImGui::PushID(id_);
    ImGui::Combo("Swap With", &newarea_, names, len);
    ImGui::Checkbox("Map", &swap_map_);
    ImGui::Checkbox("Connections", &swap_conn_);
    ImGui::Checkbox("Enemies", &swap_enemies_);
    ImGui::Checkbox("Item Avalability", &swap_itemav_);
    ImGui::Text("\n");
    ImGui::Text("Note: swapping maps does not modify the overworld connection "
        "table, or the connection table on other maps.");
    ImGui::Text("If you swap maps, you must fix connections yourself.");
    ImGui::Text("\n");
    if (ImGui::Button("Swap")) {
        Swap();
        chg = true;
    }
    ImGui::PopID();
    return chg;
}

void MapSwapper::Swap() {
    const auto& ri = ConfigLoader<RomInfo>::GetConfig();
    const Map *a = &map_;
    const Map *b = nullptr;
    AvailableBitmap avail;

    for(const auto& m : ri.map()) {
        if (m.type() != MapType::OVERWORLD
            && m.world() == map_.world()
            && m.overworld() == map_.overworld()
            && m.subworld() == map_.subworld()
            && m.area() == newarea_) {
            b = &m;
        }
    }
    for(const auto& a : ri.available()) {
        if (map_.world() == a.world()
            && map_.overworld() == a.overworld()
            && map_.subworld() == a.subworld()) {
            avail = a;
        }
    }
    LOG(INFO, "Swapping ", a->name(), " with ", b->name());

    if (swap_map_) {
        uint16_t pa = mapper_->ReadWord(a->pointer(), 0);
        uint16_t pb = mapper_->ReadWord(b->pointer(), 0);
        mapper_->WriteWord(a->pointer(), 0, pb);
        mapper_->WriteWord(b->pointer(), 0, pa);
    }
    if (swap_conn_) {
        for(int i=0; i<4; i++) {
            uint8_t ca = mapper_->Read(a->connector(), i);
            uint8_t cb = mapper_->Read(b->connector(), i);
            mapper_->Write(a->connector(), 0, cb);
            mapper_->Write(b->connector(), 0, ca);
        }
    }
    if (swap_enemies_) {
        uint16_t pa = mapper_->ReadWord(a->pointer(), 0x7e);
        uint16_t pb = mapper_->ReadWord(b->pointer(), 0x7e);
        mapper_->WriteWord(a->pointer(), 0x7e, pb);
        mapper_->WriteWord(b->pointer(), 0x7e, pa);
    }
    if (swap_itemav_) {
        uint8_t aa = mapper_->Read(avail.address(), a->area() / 2);
        uint8_t ba = mapper_->Read(avail.address(), b->area() / 2);
        uint8_t adata = aa, bdata = ba;
        // {a,b}data is the data to swap, while aa,ba is the data to keep
        // in that byte
        if (a->area() & 1) {
            adata &= 0x0F; aa &= 0xF0;
        } else {
            adata >>= 4; aa &= 0x0F;
        }
        if (b->area() & 1) {
            bdata &= 0x0F; ba &= 0xF0;
        } else {
            bdata >>= 4; ba &= 0x0F;
        }
        // Swap the data into the keeper word
        if (a->area() & 1) {
            aa |= bdata;
        } else {
            aa |= bdata << 4;
        }
        if (b->area() & 1) {
            ba |= adata;
        } else {
            ba |= adata << 4;
        }
        // Write back to memory
        mapper_->Write(avail.address(), a->area() / 2, aa);
        mapper_->Write(avail.address(), b->area() / 2, ba);
    }
    map_ = *b;
}


}  // namespace z2util
