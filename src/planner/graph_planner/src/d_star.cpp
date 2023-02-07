#include "d_star.h"

namespace d_star_planner
{
/**
 * @brief Construct a new DStar object
 *
 * @param nx          pixel number in costmap x direction
 * @param ny          pixel number in costmap y direction
 * @param resolution  costmap resolution
 */
DStar::DStar(int nx, int ny, double resolution) : global_planner::GlobalPlanner(nx, ny, resolution)
{
  global_costmap_ = new unsigned char[ns_];
  goal_.x = goal_.y = INF;
  initMap();
}

/**
 * @brief Init map
 */
void DStar::initMap()
{
  map_ = new DNodePtr*[nx_];
  for (int i = 0; i < nx_; i++)
  {
    map_[i] = new DNodePtr[ny_];
    for (int j = 0; j < ny_; j++)
      map_[i][j] = new DNode(i, j, INF, INF, this->grid2Index(i, j), -1, NEW, INF);
  }
}

/**
 * @brief Reset the system
 */
void DStar::reset()
{
  open_list_.clear();

  for (int i = 0; i < nx_; i++)
    for (int j = 0; j < ny_; j++)
      delete map_[i][j];

  for (int i = 0; i < nx_; i++)
    delete[] map_[i];

  delete[] map_;

  initMap();
}

/**
 * @brief Insert node_ptr into the open_list with h_new
 *
 * @param node_ptr  DNode pointer of the DNode to be inserted
 * @param h_new     new h value
 */
void DStar::insert(DNodePtr node_ptr, double h_new)
{
  if (node_ptr->tag == NEW)
    node_ptr->k = h_new;
  else if (node_ptr->tag == OPEN)
    node_ptr->k = std::min(node_ptr->k, h_new);
  else if (node_ptr->tag == CLOSED)
    node_ptr->k = std::min(node_ptr->cost, h_new);

  node_ptr->cost = h_new;
  node_ptr->tag = OPEN;
  open_list_.insert(std::make_pair(node_ptr->k, node_ptr));
}

/**
 * @brief Check if there is collision between n1 and n2
 *
 * @param n1  DNode pointer of one DNode
 * @param n2  DNode pointer of the other DNode
 * @return true if collision
 */
bool DStar::isCollision(DNodePtr n1, DNodePtr n2)
{
  return global_costmap_[n1->id] > lethal_cost_ * factor_ || global_costmap_[n2->id] > lethal_cost_ * factor_;
}

/**
 * @brief Get neighbour DNodePtrs of node_ptr
 *
 * @param node_ptr     DNode to expand
 * @param neighbours  neigbour DNodePtrs in vector
 */
void DStar::getNeighbours(DNodePtr node_ptr, std::vector<DNodePtr>& neighbours)
{
  int x = node_ptr->x, y = node_ptr->y;
  for (int i = -1; i <= 1; i++)
  {
    for (int j = -1; j <= 1; j++)
    {
      if (i == 0 && j == 0)
        continue;

      int x_n = x + i, y_n = y + j;
      if (x_n < 0 || x_n > nx_ || y_n < 0 || y_n > ny_)
        continue;
      DNodePtr neigbour_ptr = map_[x_n][y_n];
      if (isCollision(node_ptr, neigbour_ptr))
        continue;

      neighbours.push_back(neigbour_ptr);
    }
  }
}

/**
 * @brief Get the cost between n1 and n2, return INF if collision
 *
 * @param n1 DNode pointer of one DNode
 * @param n2 DNode pointer of the other DNode
 * @return cost between n1 and n2
 */
double DStar::getCost(DNodePtr n1, DNodePtr n2)
{
  if (isCollision(n1, n2))
    return INF;
  return std::hypot(n1->x - n2->x, n1->y - n2->y);
}

/**
 * @brief Main process of D*
 *
 * @return k_min
 */
double DStar::processState()
{
  if (open_list_.empty())
    return -1;

  double k_old = open_list_.begin()->first;
  DNodePtr x = open_list_.begin()->second;
  open_list_.erase(open_list_.begin());
  x->tag = CLOSED;

  std::vector<DNodePtr> neigbours;
  this->getNeighbours(x, neigbours);

  // RAISE state, try to reduce k value by neibhbours
  if (k_old < x->cost)
  {
    for (int i = 0; i < (int)neigbours.size(); i++)
    {
      DNodePtr y = neigbours[i];
      if (y->cost <= k_old && x->cost > y->cost + this->getCost(y, x))
      {
        x->pid = y->id;
        x->cost = y->cost + this->getCost(y, x);
      }
    }
  }

  // LOWER state, cost reductions
  if (k_old == x->cost)
  {
    for (int i = 0; i < (int)neigbours.size(); i++)
    {
      DNodePtr y = neigbours[i];
      if (y->tag == NEW || (y->pid == x->id && y->cost != x->cost + this->getCost(x, y)) ||
          (y->pid != x->id && y->cost > x->cost + this->getCost(x, y)))
      {
        y->pid = x->id;
        this->insert(y, x->cost + this->getCost(x, y));
      }
    }
  }
  else
  {
    // RAISE state
    for (int i = 0; i < (int)neigbours.size(); i++)
    {
      DNodePtr y = neigbours[i];
      if (y->tag == NEW || (y->pid == x->id && y->cost != x->cost + this->getCost(x, y)))
      {
        y->pid = x->id;
        this->insert(y, x->cost + this->getCost(x, y));
      }
      else if (y->pid != x->id && y->cost > x->cost + this->getCost(x, y))
      {
        this->insert(x, x->cost);
      }
      else if (y->pid != x->id && x->cost > y->cost + this->getCost(x, y) && y->tag == CLOSED && y->cost > k_old)
      {
        this->insert(y, y->cost);
      }
    }
  }

  return open_list_.begin()->first;
}

/**
 * @brief Extract the expanded Nodes (CLOSED)
 *
 * @param expand expanded Nodes in vector
 */
void DStar::extractExpand(std::vector<Node>& expand)
{
  for (int i = 0; i < this->nx_; i++)
  {
    for (int j = 0; j < this->ny_; j++)
    {
      DNodePtr tmp = map_[i][j];
      if (tmp->tag == CLOSED)
        expand.push_back(*tmp);
    }
  }
}

/**
 * @brief Extract path for map
 *
 * @param start start node
 * @param goal  goal node
 */
void DStar::extractPath(const Node& start, const Node& goal)
{
  DNodePtr node_ptr = map_[start.x][start.y];
  while (node_ptr->x != goal.x || node_ptr->y != goal.y)
  {
    path_.push_back(*node_ptr);

    int x, y;
    this->index2Grid(node_ptr->pid, x, y);
    node_ptr = map_[x][y];
  }
  std::reverse(path_.begin(), path_.end());
}

/**
 * @brief Get the closest Node of the path to current state
 *
 * @param current current state
 * @return the closest Node
 */
Node DStar::getState(const Node& current)
{
  Node state(path_[0].x, path_[0].y);
  int dis_min = std::hypot(state.x - current.x, state.y - current.y);
  int idx_min = 0;
  for (int i = 1; i < path_.size(); i++)
  {
    int dis = std::hypot(path_[i].x - current.x, path_[i].y - current.y);
    if (dis < dis_min)
    {
      dis_min = dis;
      idx_min = i;
    }
  }
  state.x = path_[idx_min].x;
  state.y = path_[idx_min].y;

  return state;
}

/**
 * @brief Modify the map when collision occur between x and y in path, and then do processState()
 *
 * @param x DNode pointer of one DNode
 * @param y DNode pointer of the other DNode
 */
void DStar::modify(DNodePtr x, DNodePtr y)
{
  if (x->tag == CLOSED)
    this->insert(x, y->cost + this->getCost(x, y));

  while (1)
  {
    double k_min = this->processState();
    if (k_min >= x->cost)
      break;
  }
}

/**
 * @brief D* implementation
 * @param costs   costmap
 * @param start   start node
 * @param goal    goal node
 * @param expand  containing the node been search during the process
 * @return tuple contatining a bool as to whether a path was found, and the path
 */
std::tuple<bool, std::vector<Node>> DStar::plan(const unsigned char* costs, const Node& start, const Node& goal,
                                                std::vector<Node>& expand)
{
  // update costmap
  memcpy(global_costmap_, costs, ns_);

  if (goal_.x != goal.x || goal_.y != goal.y)
  {
    this->reset();
    goal_ = goal;

    DNodePtr start_ptr = map_[start.x][start.y];
    DNodePtr goal_ptr = map_[goal.x][goal.y];

    this->insert(goal_ptr, 0);
    while (1)
    {
      processState();
      if (start_ptr->tag == CLOSED)
        break;
    }

    path_.clear();
    this->extractPath(start, goal);

    expand.clear();
    this->extractExpand(expand);
    return { true, path_ };
  }
  else
  {
    // get current state from path, argmin Euler distance
    Node state = this->getState(start);
    DNodePtr x = map_[state.x][state.y];
    DNodePtr y;

    // walk forward N points, once collision, modify
    for (int i = 0; i < SIM_DISTANCE; i++)
    {
      // goal reached
      if (x->pid == -1)
        break;

      int x_val, y_val;
      this->index2Grid(x->pid, x_val, y_val);
      y = map_[x_val][y_val];
      if (isCollision(x, y))
      {
        // ROS_WARN("Collision on original path, modified.");
        this->modify(x, y);
        continue;
      }
      x = y;
    }

    path_.clear();
    this->extractPath(state, goal);

    expand.clear();
    this->extractExpand(expand);
    return { true, path_ };
  }
}
}  // namespace d_star_planner