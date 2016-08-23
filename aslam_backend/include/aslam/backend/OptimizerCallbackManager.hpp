#ifndef OPTIMIZERCALLBACKMANAGER_HPP_
#define OPTIMIZERCALLBACKMANAGER_HPP_

#include "OptimizerCallback.hpp"
#include <typeindex>
namespace aslam {
namespace backend {
namespace callback {

class RegistryData;

class Registry {
 public:
  Registry();
  ~Registry();
  template <typename T>
  OptimizerCallback add(std::initializer_list<std::type_index> events, T callback){
    OptimizerCallback optCallback(callback);
    add(events, optCallback);
    return optCallback;
  }

  void add(std::initializer_list<std::type_index> events, const OptimizerCallback & callback);

  template <typename Event_>
  void add(const OptimizerCallback & callback){
    static_assert(std::is_base_of<Event, Event_>::value, "Only children of callback::Event are allowed as callback events!");
    add({typeid(Event_)}, callback);
  }
  void remove(std::initializer_list<std::type_index> events, const OptimizerCallback & callback);
  void remove(std::type_index event, const OptimizerCallback & callback){
    remove({event}, callback);
  }

  void clear();
  void clear(std::type_index event);

  std::size_t numCallbacks(std::type_index event) const;

 private:
  friend class Manager;
  RegistryData * data;
};

class Manager : public Registry {
 public:
  ProceedInstruction issueCallback(const Event & arg);
};

}  // namespace callback
}  // namespace backend
}  // namespace aslam

#endif /* OPTIMIZERCALLBACKMANAGER_HPP_ */
