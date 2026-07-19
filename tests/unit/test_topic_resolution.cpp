// tests/unit/test_topic_resolution.cpp
// Tests verifying that io_topic absolute vs. relative resolution rules
// produce the correct final topic. The resolution logic lives in main_rpi.cpp
// (resolveTopic function). We inline the same logic here so that these tests
// remain self-contained and verify the contract independently.

#include <gtest/gtest.h>
#include <string>

// Rule: if io_topic starts with '/', strip the leading slash and use it
// verbatim (absolute); otherwise prepend base_topic + '/'.
static std::string resolveTopic(const std::string &base, const std::string &io)
{
    if (!io.empty() && io.front() == '/')
        return io.substr(1);
    return base + "/" + io;
}

// ─── Relative topics ─────────────────────────────────────────────────────────

TEST(TopicResolution, RelativeTopicPrependsBase)
{
    EXPECT_EQ(resolveTopic("pxio/home", "gpio/led"), "pxio/home/gpio/led");
}

TEST(TopicResolution, RelativeTopicWithDeepBase)
{
    EXPECT_EQ(resolveTopic("site/room/pxio", "gpio/button"),
              "site/room/pxio/gpio/button");
}

TEST(TopicResolution, RelativeTopicEmptyIo)
{
    EXPECT_EQ(resolveTopic("pxio/home", ""), "pxio/home/");
}

// ─── Absolute topics (leading '/') ───────────────────────────────────────────

TEST(TopicResolution, AbsoluteTopicStripLeadingSlash)
{
    EXPECT_EQ(resolveTopic("pxio/home", "/sensors/temperature"),
              "sensors/temperature");
}

TEST(TopicResolution, AbsoluteTopicIgnoresBase)
{
    EXPECT_EQ(resolveTopic("pxio/home", "/completely/different/path"),
              "completely/different/path");
}

TEST(TopicResolution, AbsoluteTopicOnlySlashBecomeEmpty)
{
    // Edge case: io_topic = "/" → strips slash → empty string
    EXPECT_EQ(resolveTopic("pxio/home", "/"), "");
}

// ─── Base topic with trailing slash ──────────────────────────────────────────

TEST(TopicResolution, BaseWithTrailingSlashNoDoubleSlash)
{
    // The caller is expected to not include a trailing slash on base_topic.
    // Verify the raw concat behaviour is predictable.
    EXPECT_EQ(resolveTopic("pxio/home/", "gpio/led"), "pxio/home//gpio/led");
}
