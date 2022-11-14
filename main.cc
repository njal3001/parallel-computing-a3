#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <assert.h>
#define OMPI_SKIP_MPICXX 1
#include <mpi.h>
#include <string.h>
#include <queue>
#include <algorithm>

constexpr uint32_t num_lines = 3;

using adjacency_matrix = std::vector<std::vector<uint32_t>>;

struct Station
{
    uint32_t popularity;

    static void register_type();
    static MPI_Datatype datatype;

    Station();
};

MPI_Datatype Station::datatype = 0;
void Station::register_type()
{
    const int num_fields = 1;
    MPI_Datatype types[num_fields] =
    {
        MPI_UNSIGNED,
    };
    int block_lengths[num_fields] =
    {
        1,
    };
    MPI_Aint offsets[num_fields] =
    {
        offsetof(Station, popularity),
    };

    MPI_Type_create_struct(num_fields, block_lengths, offsets,
            types, &datatype);
    MPI_Type_commit(&datatype);
}

struct Link
{
    uint32_t src;
    uint32_t dst;
    uint32_t length;

    uint32_t next_link[num_lines];
    uint32_t prev_link[num_lines];

    Link();
    Link(uint32_t src, uint32_t dst, uint32_t length);

    static void register_type();
    static MPI_Datatype datatype;
};

MPI_Datatype Link::datatype = 0;
void Link::register_type()
{
    const int num_fields = 5;
    MPI_Datatype types[num_fields] =
    {
        MPI_UNSIGNED,
        MPI_UNSIGNED,
        MPI_UNSIGNED,
        MPI_UNSIGNED,
        MPI_UNSIGNED,
    };
    int block_lengths[num_fields] =
    {
        1,
        1,
        1,
        num_lines,
        num_lines,
    };
    MPI_Aint offsets[num_fields] =
    {
        offsetof(Link, src),
        offsetof(Link, dst),
        offsetof(Link, length),
        offsetof(Link, prev_link),
        offsetof(Link, next_link),
    };

    MPI_Type_create_struct(num_fields, block_lengths, offsets,
            types, &datatype);
    MPI_Type_commit(&datatype);
}

struct Troon {
  enum class State {
    waiting_platform,
    on_platform,
    waiting_transit,
    in_transit,
  };

  uint32_t id;
  uint32_t line;
  State state;
  uint32_t state_timestamp;
  uint32_t on_link;

  static void register_type();
  static MPI_Datatype datatype;

  Troon();
  Troon(uint32_t id, uint32_t line, uint32_t tick, uint32_t link);
};

MPI_Datatype Troon::datatype = 0;
void Troon::register_type()
{
    const int num_fields = 5;
    MPI_Datatype types[num_fields] =
    {
        MPI_UNSIGNED,
        MPI_UNSIGNED,
        MPI_INT,
        MPI_UNSIGNED,
        MPI_UNSIGNED,
    };
    int block_lengths[num_fields] =
    {
        1,
        1,
        1,
        1,
        1,
    };
    MPI_Aint offsets[num_fields] =
    {
        offsetof(Troon, id),
        offsetof(Troon, line),
        offsetof(Troon, state),
        offsetof(Troon, state_timestamp),
        offsetof(Troon, on_link),
    };

    MPI_Type_create_struct(num_fields, block_lengths, offsets,
            types, &datatype);
    MPI_Type_commit(&datatype);
}

struct Network
{
    std::vector<Station> stations;
    std::vector<Link> links;

    uint32_t line_forward_start[num_lines];
    uint32_t line_backward_start[num_lines];

    uint32_t ticks;
    uint32_t num_line_troons_total[num_lines];
    uint32_t num_print_lines;

    uint32_t num_line_troons_spawned[num_lines];

    std::vector<std::string> station_names;

    Network();

    Network(uint32_t num_stations,
            std::vector<uint32_t> &popularities, adjacency_matrix &mat,
            std::vector<std::string> &station_names,
            std::vector<std::string> &green_station_names,
            std::vector<std::string> &yellow_station_names,
            std::vector<std::string> &blue_station_names,
            uint32_t ticks, uint32_t num_green, uint32_t num_yellow,
            uint32_t num_blue, uint32_t num_print_lines);

    void print() const;

    void broadcast();
    void receive();

    size_t troon_count() const;

    const Station *src(uint32_t link_id) const;
    const Station *dst(uint32_t link_id) const;

private:
    uint32_t add_link(size_t src, size_t dst,
            const adjacency_matrix &length_mat, adjacency_matrix &link_mat);
};

struct LinkState
{
    struct CompareTroon {
        const std::vector<Troon> *troons;
        bool operator()(uint32_t index_a, uint32_t index_b)
        {
            const Troon *a = troons->data() + index_a - 1;
            const Troon *b = troons->data() + index_b - 1;
            if (a->state_timestamp != b->state_timestamp) {
              return a->state_timestamp > b->state_timestamp;
            }

            return a->id > b->id;
        }

        CompareTroon(const std::vector<Troon> *troons)
            : troons(troons)
        {}
    };

    std::priority_queue<uint32_t, std::vector<uint32_t>, CompareTroon>
        waiting_platform;
    uint32_t on_platform;
    uint32_t in_transit;

    LinkState(const std::vector<Troon> *troons);
};

struct LinkGroup
{
    std::vector<Troon> troons;
    std::vector<LinkState> link_states;
    uint32_t start;
    uint32_t end;

    LinkGroup(int rank, int num_proc, size_t num_links);

    bool has_link(uint32_t link_id) const;
    uint32_t count() const;

    LinkState *get_link_state(uint32_t link_id);
    Troon *get_troon(uint32_t index);

    friend std::ostream& operator<<(std::ostream& os, const LinkGroup& group)
    {
        os << '[' << group.start << ", " << group.end << "]\n";
        return os;
    }
};

struct TroonMessage
{
    Troon troon;
    uint32_t src_link;
    uint32_t dst_link;

    TroonMessage(const Troon &troon, uint32_t src_link, uint32_t dst_link);
    TroonMessage(uint32_t src_link, uint32_t dst_link);
};

std::vector<std::string> extract_station_names(std::string& line)
{
    constexpr char space_delimiter = ' ';
    std::vector<std::string> stations;

    line += ' ';

    size_t pos;
    while ((pos = line.find(space_delimiter)) != std::string::npos)
    {
        stations.push_back(line.substr(0, pos));
        line.erase(0, pos + 1);
    }

    return stations;
}

bool arr_contains(uint32_t val, const uint32_t *arr, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
    {
        if (val == arr[i])
        {
            return true;
        }
    }

    return false;
}

// Assumes that the array contains the value
size_t find_index(std::string &val, std::vector<std::string> &arr)
{
    for (size_t i = 0; i < arr.size(); i++)
    {
        if (val == arr[i])
        {
            return i;
        }
    }

    assert(false);
}

Station::Station()
    : popularity(0)
{}

Link::Link()
    : src(0), dst(0), length(0)
{
    for (size_t i = 0; i < num_lines; i++)
    {
        next_link[i] = 0;
        prev_link[i] = 0;
    }
}

Link::Link(uint32_t src, uint32_t dst, uint32_t length)
    : src(src), dst(dst), length(length)
{
    for (size_t i = 0; i < num_lines; i++)
    {
        next_link[i] = 0;
        prev_link[i] = 0;
    }
}

Troon::Troon()
    : id(0), line(0), state(State::waiting_platform), state_timestamp(0),
        on_link(0)
{}

Troon::Troon(uint32_t id, uint32_t line, uint32_t tick, uint32_t link)
    : id(id), line(line), state(State::waiting_platform),
        state_timestamp(tick), on_link(link)
{}

LinkState::LinkState(const std::vector<Troon> *troons)
    : waiting_platform(CompareTroon(troons)), on_platform(0), in_transit(0)
{}

LinkGroup::LinkGroup(int rank, int num_proc, size_t num_links)
{
    size_t quot = num_links / num_proc;

    start = rank * quot + 1;
    if (rank == num_proc - 1)
    {
        end = num_links + 1;
    }
    else
    {
        end = start + quot;
    }

    size_t count = end - start;
    link_states.reserve(count);
    for (size_t i = 0; i < count; i++)
    {
        link_states.emplace_back(&troons);
    }
}

bool LinkGroup::has_link(uint32_t link_id) const
{
    return link_id > 0 && start <= link_id && link_id < end;
}

uint32_t LinkGroup::count() const
{
    return end - start;
}

LinkState *LinkGroup::get_link_state(uint32_t link_id)
{
    if (!has_link(link_id))
    {
        return nullptr;
    }

    return link_states.data() + link_id - start;
}

Troon *LinkGroup::get_troon(uint32_t index)
{
    if (!index)
    {
        return nullptr;
    }

    return troons.data() + index - 1;
}

int link_rank(uint32_t link_id, uint32_t num_proc, uint32_t num_links)
{
    uint32_t quot = num_links / num_proc;
    if (!quot)
    {
        return num_proc - 1;
    }

    uint32_t rank = (link_id - 1) / quot;
    if (rank >= num_proc)
    {
        return num_proc - 1;
    }

    return rank;
}

TroonMessage::TroonMessage(const Troon &troon, uint32_t src_link,
        uint32_t dst_link)
    : troon(troon), src_link(src_link), dst_link(dst_link)
{}

TroonMessage::TroonMessage(uint32_t src_link, uint32_t dst_link)
    : src_link(src_link), dst_link(dst_link)
{}

Network::Network()
    : ticks(0), num_print_lines(0)
{
    for (size_t i = 0; i < num_lines; i++)
    {
        num_line_troons_total[i] = 0;
        num_line_troons_spawned[i] = 0;

        line_forward_start[i] = 0;
        line_backward_start[i] = 0;
    }
}

Network::Network(uint32_t num_stations,
        std::vector<uint32_t> &popularities, adjacency_matrix &mat,
        std::vector<std::string> &station_names,
        std::vector<std::string> &green_station_names,
        std::vector<std::string> &yellow_station_names,
        std::vector<std::string> &blue_station_names,
        uint32_t ticks, uint32_t num_green, uint32_t num_yellow,
        uint32_t num_blue, uint32_t num_print_lines)
    : ticks(ticks), num_print_lines(num_print_lines),
        station_names(station_names)
{
    num_line_troons_total[0] = num_green;
    num_line_troons_total[1] = num_yellow;
    num_line_troons_total[2] = num_blue;

    num_line_troons_spawned[0] = 0;
    num_line_troons_spawned[1] = 0;
    num_line_troons_spawned[2] = 0;

    stations.resize(num_stations);
    for (uint32_t i = 0; i < num_stations; i++)
    {
        stations[i].popularity = popularities[i];
    }

    // link_matrix[src][dst] is link: src -> dst
    adjacency_matrix link_matrix(num_stations,
            std::vector<uint32_t>(num_stations));

    std::vector<std::string> *all_line_names[num_lines] =
    { &green_station_names, &yellow_station_names, &blue_station_names };

    for (size_t line = 0; line < num_lines; line++)
    {
        std::vector<std::string> &line_names = *all_line_names[line];
        uint32_t num_line_stations = line_names.size();

        uint32_t forward_end_id = 0;
        uint32_t backward_end_id = 0;

        // Forward
        {
            uint32_t prev_link_id = 0;

            for (uint32_t i = 0; i < num_line_stations - 1; i++)
            {
                size_t src = find_index(line_names[i], station_names);
                size_t dst = find_index(line_names[i + 1], station_names);

                uint32_t link_id = add_link(src, dst, mat, link_matrix);
                Link &link = links[link_id - 1];

                if (!prev_link_id)
                {
                    line_forward_start[line] = link_id;
                }
                else
                {
                    link.prev_link[line] = prev_link_id;
                    links[prev_link_id - 1].next_link[line] = link_id;
                }

                prev_link_id = link_id;
            }

            forward_end_id = prev_link_id;
        }

        // Backward
        {
            uint32_t prev_link_id = 0;
            for (uint32_t i = num_line_stations - 1; i > 0; i--)
            {
                size_t src = find_index(line_names[i], station_names);
                size_t dst = find_index(line_names[i - 1], station_names);

                uint32_t link_id = add_link(src, dst, mat, link_matrix);
                Link &link = links[link_id - 1];

                if (!prev_link_id)
                {
                    line_backward_start[line] = link_id;
                }
                else
                {
                    link.prev_link[line] = prev_link_id;
                    links[prev_link_id - 1].next_link[line] = link_id;
                }

                prev_link_id = link_id;
            }

            backward_end_id = prev_link_id;
        }

        // Connect forward and backwards links
        Link &forward_start = links[line_forward_start[line] - 1];
        Link &backward_start = links[line_backward_start[line] - 1];
        Link &forward_end = links[forward_end_id - 1];
        Link &backward_end = links[backward_end_id - 1];

        forward_start.prev_link[line] = backward_end_id;
        backward_start.prev_link[line] = forward_end_id;

        forward_end.next_link[line] = line_backward_start[line];
        backward_end.next_link[line] = line_forward_start[line];
    }
}

uint32_t Network::add_link(size_t src, size_t dst,
        const adjacency_matrix &length_mat, adjacency_matrix &link_mat)
{
    uint32_t link_id = link_mat[src][dst];
    if (link_id)
    {
        return link_id;
    }

    links.push_back(Link(src, dst, length_mat[src][dst]));

    uint32_t new_link_id = links.size();
    link_mat[src][dst] = new_link_id;

    return new_link_id;
}

void Network::print() const
{
    std::cout << "Ticks: " << ticks << '\n';
    std::cout << "Green troons: " << num_line_troons_total[0]<< '\n';
    std::cout << "Yellow troons: " << num_line_troons_total[1]<< '\n';
    std::cout << "Blue troons: " << num_line_troons_total[2]<< '\n';
    std::cout << "Print lines: " << num_print_lines << '\n';

    size_t num_stations = stations.size();

    std::cout << "\nStations: ";
    for (size_t i = 0; i < num_stations; i++)
    {
        std::cout << station_names[i] << " (" <<
            stations[i].popularity << ")";

        if (i != num_stations - 1)
        {
            std::cout << ", ";
        }
    }
    std::cout << "\n\n";

    const char *line_names[num_lines] = { "Green", "Yellow", "Blue" };
    for (size_t line = 0; line < num_lines; line++)
    {
        std::cout << line_names[line] << " line: ";

        const Link *link = &links[line_forward_start[line] - 1];
        std::cout << station_names[link->src];

        uint32_t next_link_id;
        do
        {
            std::cout << " -> " <<  station_names[link->dst];

            next_link_id = link->next_link[line];
            link = &links[next_link_id - 1];
        } while (next_link_id != line_forward_start[line]);

        std::cout << '\n';
    }
}

void Network::broadcast()
{
    const int num_vals = 13;
    uint32_t vals[num_vals];

    vals[0] = stations.size();
    vals[1] = links.size();

    vals[2] = line_forward_start[0];
    vals[3] = line_forward_start[1];
    vals[4] = line_forward_start[2];

    vals[5] = line_backward_start[0];
    vals[6] = line_backward_start[1];
    vals[7] = line_backward_start[2];

    vals[8] = ticks;
    vals[9] = num_line_troons_total[0];
    vals[10] = num_line_troons_total[1];
    vals[11] = num_line_troons_total[2];
    vals[12] = num_print_lines;

    MPI_Bcast(&vals, num_vals, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    MPI_Bcast(stations.data(), stations.size(), Station::datatype, 0,
            MPI_COMM_WORLD);
    MPI_Bcast(links.data(), links.size(), Link::datatype, 0,
            MPI_COMM_WORLD);

    char name_buffer[128];
    for (auto &name : station_names)
    {
        size_t name_length = name.size() + 1;
        memcpy(name_buffer, name.c_str(), name_length);
        MPI_Bcast(name_buffer, name_length, MPI_CHAR, 0, MPI_COMM_WORLD);
    }
}

void Network::receive()
{
    const int num_vals = 13;
    uint32_t vals[num_vals];
    MPI_Bcast(&vals, num_vals, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

    uint32_t num_stations = vals[0];
    uint32_t num_links = vals[1];

    line_forward_start[0] = vals[2];
    line_forward_start[1] = vals[3];
    line_forward_start[2] = vals[4];

    line_backward_start[0] = vals[5];
    line_backward_start[1] = vals[6];
    line_backward_start[2] = vals[7];

    ticks = vals[8];
    num_line_troons_total[0] = vals[9];
    num_line_troons_total[1] = vals[10];
    num_line_troons_total[2] = vals[11];
    num_print_lines = vals[12];

    stations.resize(num_stations);
    links.resize(num_links);

    MPI_Bcast(stations.data(), stations.size(), Station::datatype, 0,
            MPI_COMM_WORLD);
    MPI_Bcast(links.data(), links.size(), Link::datatype, 0,
            MPI_COMM_WORLD);

    station_names.reserve(num_stations);
    char name_buffer[128];
    for (uint32_t i = 0; i < num_stations; i++)
    {
        MPI_Bcast(name_buffer, 128, MPI_CHAR, 0, MPI_COMM_WORLD);
        station_names.push_back(std::string(name_buffer));
    }
}

size_t Network::troon_count() const
{
    size_t count = 0;
    for (size_t line = 0; line < num_lines; line++)
    {
        count += num_line_troons_spawned[line];
    }

    return count;
}

const Station *Network::src(uint32_t link_id) const
{
    return stations.data() + links[link_id - 1].src;
}

const Station *Network::dst(uint32_t link_id) const
{
    return stations.data() + links[link_id - 1].dst;
}

void spawn_troons(Network &network, LinkGroup &link_group, uint32_t tick)
{
    for (size_t line = 0; line < num_lines; line++)
    {
        size_t spawned_line = network.num_line_troons_spawned[line];
        size_t line_total = network.num_line_troons_total[line];
        size_t left_to_spawn = line_total - spawned_line;

        size_t troon_count = network.troon_count();

        uint32_t forward_link_id = network.line_forward_start[line];
        uint32_t backward_link_id = network.line_backward_start[line];

        if (left_to_spawn > 0 &&
                link_group.has_link(forward_link_id))
        {
            Troon troon(troon_count, line, tick, forward_link_id);
            link_group.troons.push_back(troon);

            LinkState *link_state =
                link_group.get_link_state(forward_link_id);
            link_state->waiting_platform.push(link_group.troons.size());
        }
        if (left_to_spawn > 1 &&
                link_group.has_link(backward_link_id))
        {
            Troon troon(troon_count + 1, line, tick, backward_link_id);
            link_group.troons.push_back(troon);

            LinkState *link_state =
                link_group.get_link_state(backward_link_id);
            link_state->waiting_platform.push(link_group.troons.size());
        }

        network.num_line_troons_spawned[line] =
            std::min(spawned_line + 2, line_total);
    }
}

void send_all_troons(const LinkGroup &link_group)
{
    int my_count = link_group.troons.size();
    MPI_Gather(&my_count, 1, MPI_INT, NULL, 0, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Gatherv(link_group.troons.data(), my_count, Troon::datatype, nullptr,
            nullptr, nullptr, Troon::datatype, 0, MPI_COMM_WORLD);
}

void gather_all_troons(const LinkGroup &link_group, int num_proc,
        std::vector<Troon> &out)
{
    std::vector<int> troon_counts(num_proc);
    std::vector<int> offsets(num_proc);

    int my_count = link_group.troons.size();
    MPI_Gather(&my_count, 1, MPI_INT, troon_counts.data(), 1, MPI_INT, 0,
            MPI_COMM_WORLD);

    int total_count = troon_counts[0];

    for (int i = 1; i < num_proc; i++)
    {
        offsets[i] = troon_counts[i - 1] + offsets[i - 1];
        total_count += troon_counts[i];
    }

    out.resize(total_count);

    MPI_Gatherv(link_group.troons.data(), my_count, Troon::datatype, out.data(),
            troon_counts.data(), offsets.data(), Troon::datatype, 0,
            MPI_COMM_WORLD);
}

void send_troon_message(size_t index, const Network &network, int num_proc,
         std::vector<TroonMessage> &msg_buffer,
         std::vector<MPI_Request> &request_buffer)
{
    TroonMessage &msg = msg_buffer[index];
    Troon &troon = msg.troon;

    MPI_Request &req = request_buffer[index];

    int dst_rank = link_rank(msg.dst_link, num_proc, network.links.size());

    MPI_Isend(&troon, 1, Troon::datatype, dst_rank, 0,
            MPI_COMM_WORLD, &req);
}

void receive_troon_message(size_t index, const Network &network,
        int num_proc, std::vector<TroonMessage> &msg_buffer,
         std::vector<MPI_Request> &request_buffer)
{
    TroonMessage &msg = msg_buffer[index];
    Troon &troon = msg.troon;

    MPI_Request &req = request_buffer[index];

    int src_rank = link_rank(msg.src_link, num_proc, network.links.size());

    MPI_Irecv(&troon, 1, Troon::datatype, src_rank, 0,
            MPI_COMM_WORLD, &req);
}

void simulate_tick(Network &network, LinkGroup &link_group,
        uint32_t tick, int num_proc)
{
    spawn_troons(network, link_group, tick);

    std::vector<TroonMessage> send_messages;
    std::vector<MPI_Request> send_requests;

    std::vector<TroonMessage> receive_messages;
    std::vector<MPI_Request> receive_requests;

    // Sending and receiving troons
    for (uint32_t link_id = link_group.start; link_id < link_group.end;
            link_id++)
    {
        Link *link = &network.links[link_id - 1];
        LinkState *link_state = link_group.get_link_state(link_id);

        uint32_t has_sent[num_lines] = {};

        // Transit troon on link
        Troon *transit_troon = link_group.get_troon(link_state->in_transit);
        if (transit_troon &&
                tick - transit_troon->state_timestamp >= link->length)
        {
            uint32_t dst_link_id = link->next_link[transit_troon->line];
            transit_troon->state = Troon::State::waiting_platform;
            transit_troon->state_timestamp = tick;
            transit_troon->on_link = dst_link_id;

            if (link_group.has_link(dst_link_id))
            {
                // Next link is in same group, just transfer directly
                LinkState *next_link_state =
                    link_group.get_link_state(dst_link_id);

                next_link_state->waiting_platform.push(link_state->in_transit);
            }
            else
            {
                // Next link is not in the same group, need to tranfer with
                // messages
                send_messages.push_back(
                        TroonMessage(*transit_troon, link_id, dst_link_id));

                // Remove the troon since it was sent out of our group.
                // Since we do not want to invalidate any references we only
                // remove the troon if it's the last one, otherwise we just
                // mark it as invalid
                if (link_state->in_transit == link_group.troons.size())
                {
                    link_group.troons.pop_back();
                }
                else
                {
                    // Invalidate troon
                    transit_troon->on_link = 0;
                }
            }

            has_sent[transit_troon->line] = dst_link_id;
            link_state->in_transit = 0;
        }


        // We also need to notify the other connecting links that
        // no new troons have arrived
        Troon empty_troon;
        for (uint32_t line = 0; line < num_lines; line++)
        {
            uint32_t dst_link_id = link->next_link[line];

            if (!dst_link_id)
            {
                // No connecting link for this line
                continue;
            }
            if (link_group.has_link(dst_link_id))
            {
                // No need to send message if it's the same link group
                continue;
            }
            if (arr_contains(dst_link_id, has_sent, line + 1))
            {
                // We have already sent a message for this line
                continue;
            }

            send_messages.push_back(
                    TroonMessage(empty_troon, link_id, dst_link_id));

            has_sent[line] = dst_link_id;
        }

        // We need to receive all incoming troons from other links
        uint32_t has_received[num_lines] = {};
        for (uint32_t line = 0; line < num_lines; line++)
        {
            uint32_t src_link_id = link->prev_link[line];
            if (!src_link_id)
            {
                // No connecting link for this line
                continue;
            }
            if (link_group.has_link(src_link_id))
            {
                // No need to receive message if it's the same link group
                continue;
            }
            if (arr_contains(src_link_id, has_received, line + 1))
            {
                // We have already received a message for this line
                continue;
            }

            receive_messages.push_back(TroonMessage(src_link_id, link_id));

            has_received[line] = src_link_id;
        }
    }

    int send_count = send_messages.size();
    int receive_count = receive_messages.size();

    // Send all messages
    send_requests.resize(send_count);
    for (int i = 0; i < send_count; i++)
    {
        send_troon_message(i, network, num_proc, send_messages,
                send_requests);
    }

    // Receive all messages
    receive_requests.resize(receive_count);
    for (int i = 0; i < receive_count; i++)
    {
        receive_troon_message(i, network, num_proc, receive_messages,
                receive_requests);
    }

    // Wait for all receive requests to complete
    MPI_Waitall(receive_count, receive_requests.data(),
            MPI_STATUSES_IGNORE);

    // Handle the received messages
    for (auto &rec_msg : receive_messages)
    {
        Troon &arriving_troon = rec_msg.troon;

        // Ignore empty troon
        if (!arriving_troon.on_link)
        {
            continue;
        }

        // Add arriving troon to waiting platform
        link_group.troons.push_back(arriving_troon);

        LinkState *link_state =
            link_group.get_link_state(arriving_troon.on_link);

        assert(link_state);

        link_state->waiting_platform.push(link_group.troons.size());
    }

    for (uint32_t link_id = link_group.start; link_id < link_group.end;
            link_id++)
    {
        LinkState *link_state = link_group.get_link_state(link_id);

        // Move from platform to link
        if (link_state->on_platform)
        {
            Troon *platform_troon =
                link_group.get_troon(link_state->on_platform);
            if (platform_troon->state == Troon::State::waiting_transit)
            {
                platform_troon->state = Troon::State::in_transit;
                platform_troon->state_timestamp = tick;

                link_state->in_transit = link_state->on_platform;
                link_state->on_platform = 0;
            }
            else
            {
                if (!link_state->in_transit)
                {
                    // Check if troon is finished with opening
                    // and closing doors and letting passengers on
                    const Station *station = network.src(link_id);
                    if (tick - platform_troon->state_timestamp >
                            station->popularity)
                    {
                        platform_troon->state = Troon::State::waiting_transit;
                        platform_troon->state_timestamp = tick;
                    }
                }
            }
        }

        // Move from waiting area to platform
        if (!link_state->on_platform && !link_state->waiting_platform.empty())
        {
            // Get the next troon on the waiting platform,
            // skip if the troon is invalidated
            uint32_t troon_index;
            Troon *troon;

            do
            {
                troon_index = link_state->waiting_platform.top();
                troon = link_group.get_troon(troon_index);
                link_state->waiting_platform.pop();
            }
            while (!link_state->waiting_platform.empty() && !troon->on_link);

            if (troon->on_link)
            {
                link_state->on_platform = troon_index;

                troon->state = Troon::State::on_platform;
                troon->state_timestamp = tick;
            }
        }
    }

    // Wait for all send requests to complete
    MPI_Waitall(send_count, send_requests.data(),
            MPI_STATUSES_IGNORE);
}

std::string troon_name(const Troon &troon)
{
    char prefix[3] = { 'g', 'y', 'b' };
    return std::string(1, prefix[troon.line]) + std::to_string(troon.id);
}

void print_troons(std::vector<Troon> &troons, const Network &network,
        uint32_t tick)
{
    std::sort(troons.begin(), troons.end(),
            [](const Troon &a, const Troon &b)
    {
        return troon_name(a) < troon_name(b);
    });

    std::cout << tick << ": ";
    for (const auto &troon : troons)
    {
        if (!troon.on_link)
        {
            // Skip invalid troon
            continue;
        }

        std::cout << troon_name(troon);

        uint32_t src = network.links[troon.on_link - 1].src;
        std::cout << "-" << network.station_names[src];

        if (troon.state == Troon::State::in_transit)
        {
            uint32_t dst = network.links[troon.on_link - 1].dst;
            std::cout << "->" << network.station_names[dst];
        }
        else if (troon.state == Troon::State::waiting_platform)
        {
          std::cout << "#";
        }
        else
        {
          std::cout << "%";
        }

        std::cout << " ";
    }

    std::cout << '\n' << std::flush;
}

void main_proc_exec(int argc, char *argv[], int num_proc)
{
    std::vector<std::string> station_names;
    uint32_t num_stations;
    uint32_t ticks;
    uint32_t num_green;
    uint32_t num_yellow;
    uint32_t num_blue;
    uint32_t num_print_lines;

    if (argc < 2) {
        std::cerr << argv[0] << " <input_file>\n";
        std::exit(1);
    }

    std::ifstream ifs(argv[1], std::ios_base::in);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open " << argv[1] << '\n';
        std::exit(2);
    }

    ifs >> num_stations;
    std::string station_name;
    station_names.reserve(num_stations);
    for (size_t i = 0; i < num_stations; i++)
    {
        ifs >> station_name;
        station_names.emplace_back(station_name);
    }

    uint32_t popularity;
    std::vector<uint32_t> popularities;
    popularities.reserve(num_stations);
    for (size_t i = 0; i < num_stations; i++)
    {
        ifs >> popularity;
        popularities.emplace_back(popularity);
    }

    adjacency_matrix mat(num_stations, std::vector<uint32_t>(num_stations));
    for (size_t src = 0; src < num_stations; src++)
    {
        for (size_t dst = 0; dst < num_stations; dst++)
        {
            ifs >> mat[src][dst];
        }
    }

    ifs.ignore();

    std::string stations_buffer;

    std::getline(ifs, stations_buffer);
    std::vector<std::string> green_station_names =
        extract_station_names(stations_buffer);

    std::getline(ifs, stations_buffer);
    std::vector<std::string> yellow_station_names =
        extract_station_names(stations_buffer);

    std::getline(ifs, stations_buffer);
    std::vector<std::string> blue_station_names =
        extract_station_names(stations_buffer);

    ifs >> ticks;

    ifs >> num_green;
    ifs >> num_yellow;
    ifs >> num_blue;

    ifs >> num_print_lines;

    Network network(num_stations, popularities, mat, station_names,
            green_station_names, yellow_station_names, blue_station_names,
            ticks, num_green, num_yellow, num_blue, num_print_lines);

    network.broadcast();

    LinkGroup link_group(0, num_proc, network.links.size());

    std::vector<Troon> all_troons;
    for (uint32_t tick = 0; tick < network.ticks; tick++)
    {
        simulate_tick(network, link_group, tick, num_proc);

        if (network.ticks - network.num_print_lines <= tick)
        {
            gather_all_troons(link_group, num_proc, all_troons);
            print_troons(all_troons, network, tick);
        }
    }
}

void sub_proc_exec(int rank, int num_proc)
{
    Network network;
    network.receive();

    LinkGroup link_group(rank, num_proc, network.links.size());

    for (uint32_t tick = 0; tick < network.ticks; tick++)
    {
        simulate_tick(network, link_group, tick, num_proc);

        if (network.ticks - network.num_print_lines <= tick)
        {
            send_all_troons(link_group);
        }
    }
}

int main(int argc, char *argv[])
{
    int num_proc;
    int rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    Station::register_type();
    Link::register_type();
    Troon::register_type();

    if (!rank)
    {
        main_proc_exec(argc, argv, num_proc);
    }
    else
    {
        sub_proc_exec(rank, num_proc);
    }

    MPI_Finalize();
}
