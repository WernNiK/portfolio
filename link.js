document.addEventListener("DOMContentLoaded", () => {
  const CONTACT_EMAIL = "wenmarc39@gmail.com";
  const SOCIAL_LINKS = {
    linkedin: "https://www.linkedin.com/in/werner-marc-caracas-59356a3a3?lipi=urn%3Ali%3Apage%3Ad_flagship3_profile_view_base_contact_details%3Bbc45l723Rc%2BnLuDa1mjyvQ%3D%3D",
    github: "https://github.com/WernNiK",
    facebook: "https://www.facebook.com/share/1GdS9TCeC3/?mibextid=wwXIfr"
  };

  const emailLink = document.getElementById("contactEmailLink");
  const icons = document.querySelectorAll(".social-icon");

  if (emailLink) {
    emailLink.href = `mailto:${CONTACT_EMAIL}`;
  }

  icons.forEach((icon) => {
    icon.addEventListener("click", () => {
      const key = icon.getAttribute("data-social");
      const url = key ? SOCIAL_LINKS[key] : "";
      if (url) {
        window.open(url, "_blank");
      }
    });
  });
});
