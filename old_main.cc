#include <fstream>
#include <iostream>
#define OMPI_SKIP_MPICXX 1
#include <mpi.h>
#include <string>
#include <vector>
#include <queue>
#include <algorithm>
#include <string.h>
#include <unistd.h>

using std::string;
using std::vector;

using adjacency_matrix = std::vector<std::vector<size_t>>;

int min(int a, int b) {
  return a < b ? a : b;
}

struct Troon {
  enum class State {
    waiting_platform,
    on_platform,
    waiting_transit,
    in_transit,
  };

  int id;

  int line;
  State state;
  int state_timestamp;
  int on_link;

  static void registerType();
  static MPI_Datatype datatype;
};

struct Station {
  int id;
  int popularity;

  int forward_links[3];
  int backward_links[3];

  static void registerType();
  static MPI_Datatype datatype;
};

struct Link {
  enum class Usage {
    inactive,
    forward,
    backward,
  };

  int from;
  int to;
  int length;

  Usage usage[3];

  static void registerType();
  static MPI_Datatype datatype;
};

struct CompareTroon {
  bool operator()(Troon &a, Troon &b) {
    if (a.state_timestamp != b.state_timestamp) {
      return a.state_timestamp > b.state_timestamp;
    }

    return a.id > b.id;
  }
};

struct LinkState {
  std::priority_queue<Troon, std::vector<Troon>, CompareTroon> waiting_platform;
  Troon on_platform;
  Troon in_transit;

  LinkState();
};

MPI_Datatype Troon::datatype = 0;
void Troon::registerType() {
  const int num_fields = 5;
  MPI_Datatype types[num_fields] = { MPI_INT, MPI_INT, MPI_CXX_BOOL, MPI_INT, MPI_INT };
  int block_lengths[num_fields] = { 1, 1, 1, 1, 1 };
  MPI_Aint offsets[num_fields] = { offsetof(Troon, id), offsetof(Troon, line),
    offsetof(Troon, state), offsetof(Troon, state_timestamp), offsetof(Troon, on_link) };

  MPI_Type_create_struct(num_fields, block_lengths, offsets, types, &datatype);
  MPI_Type_commit(&datatype);
}

MPI_Datatype Station::datatype = 0;

void Station::registerType() {
  const int num_fields = 4;
  MPI_Datatype types[num_fields] = { MPI_INT, MPI_INT, MPI_INT, MPI_INT };
  int block_lengths[num_fields] = { 1, 1, 3, 3 };
  MPI_Aint offsets[num_fields] = { offsetof(Station, id), offsetof(Station, popularity),
    offsetof(Station, forward_links), offsetof(Station, backward_links) };

  MPI_Type_create_struct(num_fields, block_lengths, offsets, types, &datatype);
  MPI_Type_commit(&datatype);
}

MPI_Datatype Link::datatype = 0;

void Link::registerType() {
  const int num_fields = 4;

  MPI_Datatype types[num_fields] = { MPI_INT, MPI_INT, MPI_INT, MPI_INT };
  int block_lengths[num_fields] = { 1, 1, 1, 3 };
  MPI_Aint offsets[num_fields] = { offsetof(Link, from), offsetof(Link, to), offsetof(Link, length), offsetof(Link, usage)};

  MPI_Type_create_struct(num_fields, block_lengths, offsets, types, &datatype);
  MPI_Type_commit(&datatype);
}

LinkState::LinkState() {
  on_platform.id = -1;
  in_transit.id = -1;
}

std::ostream &operator <<(std::ostream &os, const Link &link) {
  os << "(" << link.from << " -> #" << link.length << " -> " << link.to << ")";
  return os;
}

vector<string> extract_station_names(string& line) {
  constexpr char space_delimiter = ' ';
  vector<string> stations{};
  line += ' ';
  size_t pos;
  while ((pos = line.find(space_delimiter)) != string::npos) {
    stations.push_back(line.substr(0, pos));
    line.erase(0, pos + 1);
  }
  return stations;
}

int main(int argc, char *argv[]) {
  using std::cout;

  int numproc, rank;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  Troon::registerType();
  Station::registerType();
  Link::registerType();

/*
#ifdef DEBUG
  if (rank == 0)
  {
    char hostname[256];
    gethostname(hostname, 256);
    bool attached = false;
    cout << "Waiting for debugger to be attached, Hostname: " << hostname << ", PID: " << getpid() << '\n';
    while (!attached) sleep(1);
  }
#endif
*/

  int ticks;
  int num_lines;
  int num_troons[3];

  Station *stations;
  int num_stations;

  Link *links;
  int num_links;

  int forward_start_links[3];
  int backward_start_links[3];


  std::vector<string> station_names{};
  if (!rank)
  {

    if (argc < 2) {
      std::cerr << argv[0] << " <input_file>\n";
      std::exit(1);
    }

    std::ifstream ifs(argv[1], std::ios_base::in);
    if (!ifs.is_open()) {
      std::cerr << "Failed to open " << argv[1] << '\n';
      std::exit(2);
    }
    // Read S
    size_t S;
    ifs >> S;

    // Read station names.
    string station;
    station_names.reserve(S);
    for (size_t i = 0; i < S; ++i) {
      ifs >> station;
      station_names.emplace_back(station);
    }

    // Read P popularity
    size_t p;
    std::vector<size_t> popularities{};
    popularities.reserve(S);
    for (size_t i = 0; i < S; ++i) {
      ifs >> p;
      popularities.emplace_back(p);
    }

    // Form adjacency mat
    adjacency_matrix mat(S, std::vector<size_t>(S));
    for (size_t src{}; src < S; ++src) {
      for (size_t dst{}; dst < S; ++dst) {
        ifs >> mat[src][dst];
      }
    }

    ifs.ignore();

    string stations_buf;

    std::getline(ifs, stations_buf);
    auto green_station_names = extract_station_names(stations_buf);

    std::getline(ifs, stations_buf);
    auto yellow_station_names = extract_station_names(stations_buf);

    std::getline(ifs, stations_buf);
    auto blue_station_names = extract_station_names(stations_buf);

    // N time ticks
    size_t N;
    ifs >> N;

    // g,y,b number of trains per line
    size_t g, y, b;
    ifs >> g;
    ifs >> y;
    ifs >> b;

    ifs >> num_lines;

    ticks = N;

    num_troons[0] = g;
    num_troons[1] = y;
    num_troons[2] = b;

    num_stations = S;

    stations = (Station*)malloc(sizeof(Station) * num_stations);
    for (int i = 0; i < num_stations; i++)
    {
      Station *station = stations + i;
      station->id = i;
      station->popularity = popularities[i];

      for (size_t j = 0; j  < 3; j++) {
        station->forward_links[j] = -1;
        station->backward_links[j] = -1;
      }
    }

    {
      int *connections = (int*)malloc(sizeof(int) * num_stations * num_stations);

      // Determine number of links
      num_links = 0;
      for (int i = 0; i < num_stations; i++) {
        for (int j = 0; j < num_stations; j++) {
          if (mat[i][j]) {
            num_links++;
          }
        }
      }

      links = (Link*)malloc(sizeof(Link) * num_links);

      // Add links to array
      int link_index = 0;
      for (int i = 0; i < num_stations; i++) {
        for (int j = 0; j < num_stations; j++) {
          int length = mat[i][j];
          if (length) {
            Link *l = links + link_index;
            l->from = i;
            l->to = j;
            l->length = length;
            l->usage[0] = Link::Usage::inactive;
            l->usage[1] = Link::Usage::inactive;
            l->usage[2] = Link::Usage::inactive;

            connections[i * num_stations + j] = link_index;
            link_index++;
          }
        }
      }

      std::vector<string> *all_line_names[3] = { &green_station_names, &yellow_station_names, &blue_station_names };
      for (int line_index = 0; line_index < 3; line_index++) {
        std::vector<string> &line_names = *all_line_names[line_index];
        int num_line_stations = line_names.size();

        Station *current_station = nullptr;
        for (int i = 0; i < num_line_stations; i++) {
          for (int j = 0; j < num_stations; j++) {
            if (line_names[i] == station_names[j]) {
              Station *next_station = stations + j;
              if (current_station) {
                int forward_link_id = connections[current_station->id * num_stations + next_station->id];
                int backward_link_id = connections[next_station->id * num_stations + current_station->id];
                current_station->forward_links[line_index] = forward_link_id;
                next_station->backward_links[line_index] = backward_link_id;

                links[forward_link_id].usage[line_index] = Link::Usage::forward;
                links[backward_link_id].usage[line_index] = Link::Usage::backward;

                // Check if first link in line
                if (current_station->backward_links[line_index] < 0) {
                  forward_start_links[line_index] = forward_link_id;
                }
              } else {
                next_station->backward_links[line_index] = -1;
              }

              current_station = next_station;
            }
          }
        }

        backward_start_links[line_index] = current_station->backward_links[line_index];
      }

      free(connections);
    }

#ifdef DEBUG
    printf("Number of ticks: %d\n", ticks);
    printf("Number of troons: [%d, %d, %d]\n", num_troons[0], num_troons[1], num_troons[2]);
    std::cout << "Starting forward links are: " << links[forward_start_links[0]] << ", " << links[forward_start_links[1]] << ", " << links[forward_start_links[2]] << '\n';
    std::cout << "Starting backward links are: " << links[backward_start_links[0]] << ", " << links[backward_start_links[1]] << ", " << links[backward_start_links[2]] << '\n';

    for (int i = 0; i < num_links; i++) {
      std::cout << "Link " << i << ": " << links[i] << '\n';
    }

    std::cout << "Stations:\n";
    for (int i = 0; i < num_stations; i++) {
      Station *station = stations + i;
      std::cout << "\tId: " << station->id << ", Popularity: " << station->popularity << ", Name: " << station_names[i];
      std::cout << ", Forward links: [";
      for (size_t j = 0; j < 3; j++) {
        if (station->forward_links[j] >= 0) {
          std::cout << links[station->forward_links[j]];
        } else {
          std::cout << "NaN";
        }
        if (j < 2) std::cout << ", ";
      }
      std::cout << "]";

      std::cout << ", Backward links: [";
      for (size_t j = 0; j < 3; j++) {
        if (station->backward_links[j] >= 0) {
          std::cout << links[station->backward_links[j]];
        } else {
          std::cout << "NaN";
        }
        if (j < 2) std::cout << ", ";
      }

      std::cout << "]\n";
    }
#endif

    MPI_Bcast(&ticks, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&num_lines, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(num_troons, 3, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&num_stations, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&num_links, 1, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Bcast(stations, num_stations, Station::datatype, 0, MPI_COMM_WORLD);
    MPI_Bcast(links, num_links, Link::datatype, 0, MPI_COMM_WORLD);

    MPI_Bcast(forward_start_links, 3, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(backward_start_links, 3, MPI_INT, 0, MPI_COMM_WORLD);

  } else {
    MPI_Bcast(&ticks, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&num_lines, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(num_troons, 3, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&num_stations, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&num_links, 1, MPI_INT, 0, MPI_COMM_WORLD);

    stations = (Station*)malloc(sizeof(Station) * num_stations);
    MPI_Bcast(stations, num_stations, Station::datatype, 0, MPI_COMM_WORLD);

    links = (Link*)malloc(sizeof(Link) * num_links);
    MPI_Bcast(links, num_links, Link::datatype, 0, MPI_COMM_WORLD);

    MPI_Bcast(forward_start_links, 3, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(backward_start_links, 3, MPI_INT, 0, MPI_COMM_WORLD);
  }

  // The links will be split evenly between the processes and each process will be
  // responsible for updating their own links

  int link_group_size = (num_links + numproc - 1) / numproc;
  int my_links_start = min(rank * link_group_size, num_links);
  int my_links_end = min(my_links_start + link_group_size, num_links);
  int my_links_count = my_links_end - my_links_start;

#ifdef DEBUG
  printf("Rank %d starting at link %d to link %d\n", rank, my_links_start, my_links_end);
#endif


  if (my_links_count) {
    std::vector<LinkState> link_states(my_links_count);

    std::vector<std::pair<MPI_Request, Troon>> sent_troons;
    Troon empty_troon;
    empty_troon.id = -1;
    MPI_Request empty_request;

    // Determine if this link group should be spawning troons
    int num_forward_to_spawn[3];
    int num_backward_to_spawn[3];
    memset(num_forward_to_spawn, 0, sizeof(int) * 3);
    memset(num_backward_to_spawn, 0, sizeof(int) * 3);
    for (int i = 0; i < 3; i++) {
      int forward_spawn_link = forward_start_links[i];
      if (forward_spawn_link >= my_links_start && forward_spawn_link < my_links_end) {
        num_forward_to_spawn[i] = (num_troons[i] + 1) / 2;
      }

      int backward_spawn_link = backward_start_links[i];
      if (backward_spawn_link >= my_links_start && backward_spawn_link < my_links_end) {
        num_backward_to_spawn[i] = num_troons[i] / 2;
      }
    }

#ifdef DEBUG
    printf("Rank %d will spawn (forward: [%d, %d, %d], backward: [%d, %d, %d]\n",
      rank, num_forward_to_spawn[0], num_forward_to_spawn[1], num_forward_to_spawn[2],
      num_backward_to_spawn[0], num_backward_to_spawn[1], num_backward_to_spawn[2]);
#endif

    for (int tick = 0; tick < ticks; tick++) {
      // Spawn troons
      {
        // Calculate the total number of troons across all processes
        // Need this to create correct ids
        int troon_count = 0;
        int troons_left_to_spawn[3];
        for (int i = 0; i < 3; i++) {
          int troon_line_count = min(num_troons[i], 2 * tick);
          troons_left_to_spawn[i] = num_troons[i] - troon_line_count;
          troon_count += troon_line_count;
        }

        int forward_id = troon_count;
        for (int i = 0; i < 3; i++) {
          // Offset based on which line it is
          if (i) {
            forward_id += min(2, troons_left_to_spawn[i - 1]);
          }

          if (num_forward_to_spawn[i]) {
            Troon troon;
            troon.id = forward_id;
            troon.line = i;
            troon.state = Troon::State::waiting_platform;
            troon.state_timestamp = 0;
            troon.on_link = forward_start_links[i];

            LinkState &lstate = link_states[forward_start_links[i] - my_links_start];
            lstate.waiting_platform.push(troon);

            num_forward_to_spawn[i]--;

#ifdef DEBUG
            printf("Rank %d spawned forward troon (id: %d, line: %d) at tick %d\n", rank, troon.id, troon.line, tick);
#endif
          }
          if (num_backward_to_spawn[i]) {
            Troon troon;
            troon.id = forward_id + 1;
            troon.line = i;
            troon.state = Troon::State::waiting_platform;
            troon.state_timestamp = 0;
            troon.on_link = backward_start_links[i];

            LinkState &lstate = link_states[backward_start_links[i] - my_links_start];
            lstate.waiting_platform.push(troon);

            num_backward_to_spawn[i]--;

#ifdef DEBUG
            printf("Rank %d spawned backward troon (id: %d, line: %d) at tick %d\n", rank, troon.id, troon.line, tick);
#endif
          }
        }
      }

      int num_receive = 0;

      // Transit troons on all links
      for (int i = 0; i < my_links_count; i++) {
        Link &link = links[i + my_links_start];
        LinkState &link_state = link_states[i];

        Station &to_station = stations[link.to];

        for (int line_index = 0; line_index < 3; line_index++) {
          if (link.usage[line_index] != Link::Usage::inactive) {
            sent_troons.push_back(std::pair<MPI_Request, Troon>());
            std::pair<MPI_Request, Troon> &send_record = sent_troons.back();

            num_receive++;

            // Determine next link on line, switch direction if last link
            int next_link_id;
            if (link.usage[line_index] == Link::Usage::forward) {
              if (to_station.forward_links[line_index] < 0) {
                next_link_id = to_station.backward_links[line_index];
              } else {
                next_link_id = to_station.forward_links[line_index];
              }
            } else {
              if (to_station.backward_links[line_index] < 0) {
                next_link_id = to_station.forward_links[line_index];
              } else {
                next_link_id = to_station.backward_links[line_index];
              }
            }

            int destination_rank = next_link_id / link_group_size;

            Troon troon = link_state.in_transit;
            if (troon.id >= 0 && troon.line == line_index && tick - troon.state_timestamp >= link.length) {
              link_state.in_transit.id = -1;

              troon.state = Troon::State::waiting_platform;
              troon.state_timestamp = tick;
              troon.on_link = next_link_id;
              send_record.second = troon;

              // std::cout << "Sending troon " << troon.id << " from " << station_names[link.from] << " to " << station_names[link.to] << '\n';
#ifdef DEBUG
              printf("Sending troon from rank %d to rank %d (id: %d, line: %d, from link: %d, to link: %d) at tick %d\n",
                rank, destination_rank, troon.id, troon.line, i + my_links_start, next_link_id, tick);
#endif
              // Non blocking send to avoid deadlocks
              MPI_Isend(&send_record.second, 1, Troon::datatype, destination_rank, 0, MPI_COMM_WORLD, &send_record.first);
            } else {
              // Send invalid troon to next link, signaling that there is no troon arriving
              MPI_Isend(&empty_troon, 1, Troon::datatype, destination_rank, 0, MPI_COMM_WORLD, &empty_request);

  #ifdef DEBUG
                printf("Sending empty troon from rank %d to rank %d (line: %d, from link: %d, to link: %d) at tick %d\n",
                  rank, destination_rank, line_index, i + my_links_start, next_link_id, tick);
  #endif
            }
          }
        }
      }

#ifdef DEBUG
      printf("Rank %d is expecting %d messages\n", rank, num_receive);
#endif

      for (int i = 0; i < num_receive; i++) {
          MPI_Status status;
          Troon arriving_troon;
          arriving_troon.id = -1;
          MPI_Recv(&arriving_troon, 1, Troon::datatype, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
          if (arriving_troon.id >= 0) {
              LinkState &link_state = link_states[arriving_troon.on_link - my_links_start];
              link_state.waiting_platform.push(arriving_troon);

#ifdef DEBUG
              printf("Added troon to waiting platform at  rank %d (id: %d, line: %d, link: %d) at tick %d\n",
                rank, arriving_troon.id, arriving_troon.line, arriving_troon.on_link, tick);
#endif
          }
      }

      for (int i = 0; i < my_links_count; i++) {
        Link &link = links[i + my_links_start];
        LinkState &link_state = link_states[i];

        Station &from_station = stations[link.from];

        // Move from platform to link
        if (link_state.on_platform.id >= 0) {
          if (link_state.on_platform.state == Troon::State::waiting_transit) {
            link_state.in_transit = link_state.on_platform;
            link_state.in_transit.state = Troon::State::in_transit;
            link_state.in_transit.state_timestamp = tick;

            link_state.on_platform.id = -1;

#ifdef DEBUG
            printf("Rank %d moved troon at link %d from waiting on transit to transiting (id: %d, line: %d) at tick %d\n", rank, i + my_links_start, link_state.on_platform.id, link_state.on_platform.line, tick);
#endif
          } else {
            if (link_state.in_transit.id < 0) {
              int wait_time = from_station.popularity + 1;
              if (tick - link_state.on_platform.state_timestamp >= wait_time) {
                link_state.on_platform.state = Troon::State::waiting_transit;
                link_state.on_platform.state_timestamp = tick;

#ifdef DEBUG
              printf("Rank %d moved troon at link %d from being on platform to waiting for transit"
                "(id: %d, line: %d) at tick %d\n", rank,
                  i + my_links_start, link_state.on_platform.id, link_state.on_platform.line, tick);
#endif
              }
            }
          }
        }

        // Move from waiting area to platform
        if (link_state.on_platform.id < 0 && !link_state.waiting_platform.empty()) {
          Troon first_troon = link_state.waiting_platform.top();
          link_state.waiting_platform.pop();

          first_troon.state_timestamp = tick;
          first_troon.state = Troon::State::on_platform;
          link_state.on_platform = first_troon;

#ifdef DEBUG
          printf("Rank %d moved troon at link %d from waiting area to platform (id: %d, line: %d) at tick %d\n", rank,
              i + my_links_start, link_state.on_platform.id, link_state.on_platform.line, tick);
#endif
        }
      }

      MPI_Barrier(MPI_COMM_WORLD);
      sent_troons.clear();

      if (!rank) {
        printf("Finished tick %d\n", tick);
      }

      if (ticks - tick <= num_lines) {
        std::vector<Troon> my_troons;
        for (LinkState &link_state : link_states) {
          if (link_state.on_platform.id >= 0) {
            my_troons.push_back(link_state.on_platform);
          }
          if (link_state.in_transit.id >= 0) {
            my_troons.push_back(link_state.in_transit);
          }
          std::priority_queue<Troon, std::vector<Troon>, CompareTroon> tmp = link_state.waiting_platform;
          while (!tmp.empty()) {
            my_troons.push_back(tmp.top());
            tmp.pop();
          }
        }

        if (rank) {
          int count = my_troons.size();
          if (count > 0) {
            MPI_Send(my_troons.data(), count, Troon::datatype, 0, 0, MPI_COMM_WORLD);
          } else {
            MPI_Send(NULL, 0, Troon::datatype, 0, 0, MPI_COMM_WORLD);
          }
        } else {
          int num_senders = ((num_links + link_group_size - 1) / link_group_size) - 1;
          for (int i = 0; i < num_senders; i++) {
            MPI_Status status;
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            int count;
            MPI_Get_count(&status, Troon::datatype, &count);
            if (count > 0) {
              Troon *other_troons = (Troon*)malloc(count * sizeof(Troon));
              MPI_Recv(other_troons, count, Troon::datatype, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
              for (int i = 0; i < count; i++) {
                my_troons.push_back(other_troons[i]);
              }
              free(other_troons);
            } else {
              MPI_Recv(NULL, 0, Troon::datatype, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
          }

          std::sort(my_troons.begin(), my_troons.end(),
            [](const Troon &a, const Troon &b) {
              if (a.line != b.line) {
                if (a.line == 2) return true;
                if (a.line == 1) return false;

                return b.line != 2;
              }

              return std::to_string(a.id) < std::to_string(b.id);
          });

          std::cout << tick << ": ";
          for (Troon &troon : my_troons) {
            char lc = '\0';
            switch (troon.line) {
              case 0:
                lc = 'g';
                break;
              case 1:
                lc = 'y';
                break;
              case 2:
                lc = 'b';
                break;
            }

            std::cout << lc << troon.id << "-" << station_names[links[troon.on_link].from];
            if (troon.state == Troon::State::in_transit) {
              std::cout << "->" << station_names[links[troon.on_link].to];
            } else if (troon.state == Troon::State::waiting_platform) {
              std::cout << "#";
            } else {
              std::cout << "%";
            }

            std::cout << " ";
          }
          std::cout << '\n';
        }

        MPI_Barrier(MPI_COMM_WORLD);
      }
    }
  } else {
    for (int tick = 0; tick < ticks; tick++) {
      MPI_Barrier(MPI_COMM_WORLD);

      if (ticks - tick <= num_lines) {
        MPI_Barrier(MPI_COMM_WORLD);
      }
    }
  }

  free(stations);
  free(links);
  MPI_Finalize();

  return 0;
}
