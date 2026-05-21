const navLinks = document.querySelectorAll('.nav-links a[href^="#"]');
const sections = Array.from(navLinks)
    .map((link) => document.querySelector(link.getAttribute('href')))
    .filter(Boolean);

const setActiveLink = (id) => {
    navLinks.forEach((link) => {
        link.classList.toggle('active', link.getAttribute('href') === `#${id}`);
    });
};

const updateActiveByScroll = () => {
    if (!sections.length) return;

    const marker = window.scrollY + window.innerHeight * 0.35;
    let current = sections[0];

    sections.forEach((section) => {
        if (marker >= section.offsetTop) {
            current = section;
        }
    });

    if (current?.id) {
        setActiveLink(current.id);
    }
};

if (sections.length) {
    navLinks.forEach((link) => {
        link.addEventListener('click', () => {
            const targetId = link.getAttribute('href')?.replace('#', '');
            if (targetId) setActiveLink(targetId);
        });
    });

    window.addEventListener('scroll', updateActiveByScroll, { passive: true });
    window.addEventListener('resize', updateActiveByScroll);
    updateActiveByScroll();
}
