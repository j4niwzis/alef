// Unicode's emoji, as emoji-test.txt lists them: read while alef.emoji's
// interface is compiled, checked here against what the file says.
import std;
import alef.emoji;
import gtest;

#include "gtest/gtest-macros.h"

namespace {

// Made at compile time: a constant expression.
static_assert(alef::emoji_groups.size() == 9);
static_assert(alef::emoji_all.front().name == "grinning face");

TEST(Emoji, TheFirstIsTheGrinningFaceInItsGroup) {
  ASSERT_FALSE(alef::emoji_all.empty());
  EXPECT_EQ(alef::emoji_all.front().text, "\xf0\x9f\x98\x80");
  EXPECT_EQ(alef::emoji_groups.front().name, "Smileys & Emotion");
  EXPECT_EQ(alef::emoji_groups.back().name, "Flags");
}

TEST(Emoji, TheGroupsHoldThemAllAndNoComponents) {
  std::size_t total = 0;
  for (const alef::emoji_group &group : alef::emoji_groups) {
    EXPECT_NE(group.name, "Component");
    EXPECT_FALSE(group.all.empty()) << group.name;
    total += group.all.size();
  }
  EXPECT_EQ(total, alef::emoji_all.size());
  EXPECT_GT(total, 3000u);
}

TEST(Emoji, ASkinTonesVariantIsMarkedAndBuiltOnItsBase) {
  const auto named = [](std::string_view name) {
    return std::ranges::find(alef::emoji_all, name, &alef::emoji::name);
  };
  const auto base = named("waving hand");
  const auto toned = named("waving hand: light skin tone");
  ASSERT_NE(base, alef::emoji_all.end());
  ASSERT_NE(toned, alef::emoji_all.end());
  EXPECT_FALSE(base->toned);
  EXPECT_TRUE(toned->toned);
  EXPECT_TRUE(toned->text.starts_with(base->text.substr(0, 4)));
}

} // namespace
