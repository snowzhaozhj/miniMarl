#include "marl/event.hpp"

#include "marl_test.hpp"

#include "marl/defer.hpp"
#include "marl/wait_group.hpp"

using namespace std::chrono_literals;

class EventTestWithBound : public WithBoundScheduler {};

INSTANTIATE_WithBoundSchedulerTest(EventTestWithBound);

TEST_P(EventTestWithBound, IsSignalled) {
  for (auto mode : {marl::Event::Mode::Manual, marl::Event::Mode::Auto}) {
    auto event = marl::Event(mode);
    EXPECT_EQ(event.isSignalled(), false);
    event.signal();
    EXPECT_EQ(event.isSignalled(), true);
    EXPECT_EQ(event.isSignalled(), true);
    event.clear();
    EXPECT_EQ(event.isSignalled(), false);
  }
}

TEST_P(EventTestWithBound, AutoTest) {
  auto event = marl::Event(marl::Event::Mode::Auto);
  EXPECT_EQ(event.test(), false);
  event.signal();
  EXPECT_EQ(event.test(), true);
  EXPECT_EQ(event.test(), false);
}

TEST_P(EventTestWithBound, ManualTest) {
  auto event = marl::Event(marl::Event::Mode::Manual);
  EXPECT_EQ(event.test(), false);
  event.signal();
  EXPECT_EQ(event.test(), true);
  EXPECT_EQ(event.test(), true);
}

TEST_P(EventTestWithBound, AutoWait) {
  std::atomic<int> counter{0};
  auto event = marl::Event(marl::Event::Mode::Auto);
  auto done = marl::Event(marl::Event::Mode::Auto);

  for (int i = 0; i < 3; ++i) {
    marl::schedule([=, &counter] {
      event.wait();
      ++counter;
      done.signal();
    });
  }

  EXPECT_EQ(counter.load(), 0);
  event.signal();
  done.wait();
  EXPECT_EQ(counter.load(), 1);
  event.signal();
  done.wait();
  EXPECT_EQ(counter.load(), 2);
  event.signal();
  done.wait();
  EXPECT_EQ(counter.load(), 3);
}

TEST_P(EventTestWithBound, ManualWait) {
  std::atomic<int> counter{0};
  auto event = marl::Event(marl::Event::Mode::Manual);
  auto wg = marl::WaitGroup(3);
  for (int i = 0; i < 3; ++i) {
    marl::schedule([=, &counter] {
      event.wait();
      ++counter;
      wg.done();
    });
  }
  event.signal();
  wg.wait();
  EXPECT_EQ(counter.load(), 3);
}

TEST_P(EventTestWithBound, Sequence) {
  for (auto mode : {marl::Event::Mode::Manual, marl::Event::Mode::Auto}) {
    std::string sequence;
    auto eventA = marl::Event(mode);
    auto eventB = marl::Event(mode);
    auto eventC = marl::Event(mode);
    auto done = marl::Event(mode);
    marl::schedule([=, &sequence] {
      eventB.wait();
      sequence += "B";
      eventC.signal();
    });
    marl::schedule([=, &sequence] {
      eventA.wait();
      sequence += "A";
      eventB.signal();
    });
    marl::schedule([=, &sequence] {
      eventC.wait();
      sequence += "C";
      done.signal();
    });
    ASSERT_EQ(sequence, "");
    eventA.signal();
    done.wait();
    ASSERT_EQ(sequence, "ABC");
  }
}

TEST_P(EventTestWithBound, WaitForNoTimeout) {
  auto event = marl::Event(marl::Event::Mode::Manual);
  auto wg = marl::WaitGroup(1000);
  for (int i = 0; i < 1000; i++) {
    marl::schedule([=] {
      defer(wg.done());
      EXPECT_TRUE(event.wait_for(3s));
    });
  }
  event.signal();
  wg.wait();
}

TEST_P(EventTestWithBound, WaitForTimeout) {
  auto event = marl::Event(marl::Event::Mode::Manual);
  auto wg = marl::WaitGroup(1000);
  for (int i = 0; i < 1000; i++) {
    marl::schedule([=] {
      defer(wg.done());
      EXPECT_FALSE(event.wait_for(10ms));
    });
  }
  wg.wait();
}

TEST_P(EventTestWithBound, WaitUntilNoTimeout) {
  auto event = marl::Event(marl::Event::Mode::Manual);
  auto wg = marl::WaitGroup(1000);
  for (int i = 0; i < 1000; i++) {
    marl::schedule([=] {
      defer(wg.done());
      EXPECT_TRUE(event.wait_until(timeLater(3s)));
    });
  }
  event.signal();
  wg.wait();
}

TEST_P(EventTestWithBound, WaitUntilTimeout) {
  auto event = marl::Event(marl::Event::Mode::Manual);
  auto wg = marl::WaitGroup(1000);
  for (int i = 0; i < 1000; i++) {
    marl::schedule([=] {
      defer(wg.done());
      EXPECT_FALSE(event.wait_until(timeLater(10ms)));
    });
  }
  wg.wait();
}

TEST_P(EventTestWithBound, WaitStressTest) {
  auto event = marl::Event(marl::Event::Mode::Manual);
  for (int i = 0; i < 10; ++i) {
    auto wg = marl::WaitGroup(100);
    for (int j = 0; j < 100; ++j) {
      marl::schedule([=] {
        defer(wg.done());
        event.wait_for(std::chrono::microseconds(j));
      });
    }
    std::this_thread::sleep_for(50ms);
    event.signal();
    wg.wait();
  }
}

TEST_P(EventTestWithBound, Any) {
  for (int i = 0; i < 3; i++) {
    std::array<marl::Event, 3> events = {
        marl::Event(marl::Event::Mode::Auto),
        marl::Event(marl::Event::Mode::Auto),
        marl::Event(marl::Event::Mode::Auto),
    };
    auto any = marl::Event::any(events.begin(), events.end());
    events[i].signal();
    ASSERT_TRUE(any.isSignalled());
  }
}

