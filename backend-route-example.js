// Example Express route: serve public profile links from server-side config/env.
// Mount this in your backend so the frontend can call GET /api/public-profile
// and avoid hardcoding private links in index.html.
//
// app.get("/api/public-profile", publicProfileHandler);

function publicProfileHandler(_req, res) {
  res.json({
    contact: {
      email: process.env.PUBLIC_CONTACT_EMAIL || ""
    },
    socials: {
      linkedin: process.env.PUBLIC_LINKEDIN_URL || "",
      github: process.env.PUBLIC_GITHUB_URL || "",
      facebook: process.env.PUBLIC_FACEBOOK_URL || ""
    }
  });
}

module.exports = { publicProfileHandler };
