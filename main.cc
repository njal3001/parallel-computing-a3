#include <fstream>
#include <iostream>
#define OMPI_SKIP_MPICXX 1
#include <mpi.h>
#include <string>
#include <vector>
#include <string.h>

using std::string;
using std::vector;

using adjacency_matrix = std::vector<std::vector<size_t>>;

int min(int a, int b) {
  return a < b ? a : b;
}

struct Troon
{
  int id;

  static void registerType();
  static MPI_Datatype datatype;
};

struct Station
{
  int id;
  int popularity;

  int forward_links[3];
  int backward_links[3];

  static void registerType();
  static MPI_Datatype datatype;
};

struct Link
{
  int from;
  int to;
  int length;

  static void registerType();
  static MPI_Datatype datatype;
};

MPI_Datatype Troon::datatype = 0;

void Troon::registerType()
{
  const int num_fields = 1;
  MPI_Datatype types[num_fields] = { MPI_INT };
  int block_lengths[num_fields] = { 1 };
  MPI_Aint offsets[num_fields] = { offsetof(Troon, id) };

  MPI_Type_create_struct(num_fields, block_lengths, offsets, types, &datatype);
  MPI_Type_commit(&datatype);
}

MPI_Datatype Station::datatype = 0;

void Station::registerType()
{
  const int num_fields = 4;
  MPI_Datatype types[num_fields] = { MPI_INT, MPI_INT, MPI_INT, MPI_INT };
  int block_lengths[num_fields] = { 1, 1, 3, 3 };
  MPI_Aint offsets[num_fields] = { offsetof(Station, id), offsetof(Station, popularity),
    offsetof(Station, forward_links), offsetof(Station, backward_links) };

  MPI_Type_create_struct(num_fields, block_lengths, offsets, types, &datatype);
  MPI_Type_commit(&datatype);
}

MPI_Datatype Link::datatype = 0;

void Link::registerType()
{
  const int num_fields = 3;
  MPI_Datatype types[num_fields] = { MPI_INT, MPI_INT, MPI_INT };
  int block_lengths[num_fields] = { 1, 1, 1 };
  MPI_Aint offsets[num_fields] = { offsetof(Link, from), offsetof(Link, to), offsetof(Link, length) };

  MPI_Type_create_struct(num_fields, block_lengths, offsets, types, &datatype);
  MPI_Type_commit(&datatype);
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

  Station *stations;
  int num_stations;

  Link *links;
  int num_links;

  int forward_start_links[3];
  int backward_start_links[3];

  int num_troons[3];

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
    std::vector<string> station_names{};
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

    size_t num_lines;
    ifs >> num_lines;

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

#ifndef DEBUG
    printf("Number of troons: [%d, %d, %d]\n", num_troons[0], num_troons[1], num_troons[2]);
    std::cout << "Starting forward links are: " << links[forward_start_links[0]] << ", " << links[forward_start_links[1]] << ", " << links[forward_start_links[2]] << '\n';
    std::cout << "Starting backward links are: " << links[backward_start_links[0]] << ", " << links[backward_start_links[1]] << ", " << links[backward_start_links[2]] << '\n';

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

    MPI_Bcast(num_troons, 3, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&num_stations, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&num_links, 1, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Bcast(stations, num_stations, Station::datatype, 0, MPI_COMM_WORLD);
    MPI_Bcast(links, num_links, Link::datatype, 0, MPI_COMM_WORLD);

    MPI_Bcast(forward_start_links, 3, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(backward_start_links, 3, MPI_INT, 0, MPI_COMM_WORLD);

  } else {
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
  int my_links_start = rank * link_group_size;
  int my_links_end = min(my_links_start + link_group_size, num_links);

  printf("Rank %d starting at link %d to link %d\n", rank, my_links_start, my_links_end);

  free(stations);
  free(links);
  MPI_Finalize();

  return 0;
}
